#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/rpmsg.h>
#include <linux/mod_devicetable.h>
#include <linux/radix-tree.h>
#include <linux/mutex.h>
#include <linux/skbuff.h>
#include <linux/lguest_rpmsg.h>

/* vim: set ts=8 sts=8 sw=8 noet: */

#define RPMSG_NS_ADDR 53

static unsigned long timeout_delay;

struct lgr_channel;

static void receive_message(struct rpmsg_channel *rpdev, void *data,
		int len, void *priv, u32 src);

static int enqueue_rx_message(struct lgr_channel *lgr, struct rpmsg_hdr *msg);

static struct lgr_endpoint *translate_guest_address(struct lgr_channel *lgr,
		u32 guest_addr);

static int initialize_driver(struct lgr_channel *lgr,
		const char __user *channel);

/* An open channel connection. */
struct lgr_channel {
	/* The channel we're connected to. */
	struct rpmsg_channel *channel;
	/* A lock to protect fields in this struct. */
	struct mutex lock;
	/* A radix tree mapping guest-side rpmsg addresses to endpoints. */
	struct radix_tree_root guest_epts;

	/* Reads waiting for a message to arrive. */
	wait_queue_head_t rx_waiters;
	/* The queue of messages that have not been returned to userspace. */
	struct sk_buff_head rx_queue;

	/* The driver that gets probed by the rpmsg bus. */
	struct rpmsg_driver channel_driver;

	/* The ID table for the driver - contains one entry with the name of
	 * the channel we want to respond to. */
	struct rpmsg_device_id id_table[2];
	/* True once our driver has been registered. (We might not necessarily
	 * have been probed yet.) */
	bool driver_registered;
	/* True once our driver has been probed, and the channel is open. */
	bool initialized;
	/* Reads and writes waiting for our driver to be probed. */
	struct completion driver_probed;
};

struct lgr_endpoint {
	struct rpmsg_endpoint *ept;
	struct lgr_channel *channel;
	u32 guest_addr;
	u32 host_addr;
	bool is_ns_ept;
	char ns_name[RPMSG_NAME_SIZE];
};


static struct lgr_endpoint *translate_guest_address(
		struct lgr_channel *lgr, u32 guest_addr)
{
	struct lgr_endpoint *lgr_ept = NULL;
	struct rpmsg_endpoint *ept = NULL;
	int rc;

	if (mutex_lock_interruptible(&lgr->lock))
		return ERR_PTR(-ERESTARTSYS);

	lgr_ept = radix_tree_lookup(&lgr->guest_epts, guest_addr);

	if (!lgr_ept) {
		lgr_ept = kzalloc(sizeof(*lgr_ept), GFP_KERNEL);
		if (!lgr_ept) {
			rc = -ENOMEM;
			goto out;
		}

		/* We didn't find an endpoint for that address - make one. */
		ept = rpmsg_create_ept(lgr->channel, receive_message,
				lgr_ept, RPMSG_ADDR_ANY);

		if (!ept) {
			rc = -ENOMEM;
			goto free;
		}

		lgr_ept->ept = ept;
		lgr_ept->guest_addr = guest_addr;
		lgr_ept->host_addr = ept->addr;
		lgr_ept->is_ns_ept = false;

		rc = radix_tree_insert(&lgr->guest_epts, guest_addr, lgr_ept);

		if (rc)
			goto destroy_ept;

	}

	mutex_unlock(&lgr->lock);
	return lgr_ept;

destroy_ept:
	rpmsg_destroy_ept(ept);
free:
	kfree(lgr_ept);
out:
	mutex_unlock(&lgr->lock);
	return ERR_PTR(rc);
}

static int rpdrv_probe(struct rpmsg_channel *rpdev)
{
	struct device_driver *drv = rpdev->dev.driver;
	struct lgr_channel *lgr = 
		container_of(drv, struct lgr_channel, channel_driver.drv);
	struct rpmsg_hdr *hdr;
	struct rpmsg_ns_msg *ns;

	char msg[sizeof(*hdr) + sizeof(*ns)];

	hdr = (struct rpmsg_hdr *)msg;
	ns = (struct rpmsg_ns_msg *)hdr->data;

	/*
	 * Capture the channel - getting this pointer is the whole point
	 * of the whole fake-device-driver shenanigans.
	 */
	lgr->channel = rpdev;

	/* Mark our lgr_channel as ready. */
	lgr->initialized = true;
	/* Ensure that other threads see the change. */
	wmb();

	/* Wake up syscalls waiting for the channel to be ready. */
	complete_all(&lgr->driver_probed);

	/* Send a name-service message advertising the requested channel. */

	memcpy(ns->name, lgr->id_table[0].name, RPMSG_NAME_SIZE);
	ns->addr = rpdev->dst;
	ns->flags = RPMSG_NS_CREATE;

	hdr->len = sizeof(*ns);
	hdr->flags = 0;
	hdr->src = rpdev->src;
	hdr->dst = RPMSG_NS_ADDR;
	hdr->reserved = 0;

	return enqueue_rx_message(lgr, hdr);
}

static void rpdrv_remove(struct rpmsg_channel *rpdev)
{
	/* 
	 * The only time we should hit here is in close() via
	 * unregister_rpmsg_driver. Since close() handles all of
	 * the work for us, we do nothing here.
	 */
}

/* Enqueue the rpmsg message @msg into the RX message queue. */
static int enqueue_rx_message(struct lgr_channel *lgr, struct rpmsg_hdr *msg)
{
	struct sk_buff *skb;
	size_t len = msg->len + sizeof(*msg);

	skb = alloc_skb(len, GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	memcpy(skb_put(skb, len), msg, len);

	skb_queue_tail(&lgr->rx_queue, skb);
	wake_up_interruptible(&lgr->rx_waiters);

	return 0;
}

static void receive_message(struct rpmsg_channel *rpdev, void *data,
		int len, void *priv, u32 src)
{
	struct lgr_endpoint *ept = priv;
	struct lgr_channel *lgr;
	char msg_buf[512];
	struct rpmsg_hdr *hdr = (struct rpmsg_hdr *)msg_buf;

	if (!priv) {
		/*
		 * Private data not set means we got a message on the endpoint
		 * that rpmsg set up for us when we got probed.
		 *
		 * We don't care about that endpoint, though, so we drop
		 * the message on the floor.
		 */
		dev_warn(&rpdev->dev, 
			"Message on default endpoint? (from: %u)\n", src);

		print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
				data, len, true);

		return;
	}

	lgr = ept->channel;

	hdr->len = min((ARRAY_SIZE(msg_buf) - sizeof(*hdr)), (size_t)len);
	hdr->flags = 0;
	hdr->src = src;
	/* Reverse-translate dest address. */
	hdr->dst = ept->guest_addr;
	hdr->reserved = 0;
	memcpy(hdr->data, data, hdr->len);

	WARN_ON(enqueue_rx_message(lgr, hdr));
}

static int initialize_driver(struct lgr_channel *lgr, const char __user *channel)
{
	int rc;

	struct rpmsg_driver *rpdrv = &lgr->channel_driver;
	rpdrv->drv.name  = KBUILD_MODNAME;
	rpdrv->drv.owner = THIS_MODULE;
	rpdrv->id_table  = lgr->id_table;
	rpdrv->probe	 = rpdrv_probe;
	rpdrv->callback	 = receive_message;
	rpdrv->remove	 = __devexit_p(rpdrv_remove);

	rc = copy_from_user(&lgr->id_table[0].name, channel, RPMSG_NAME_SIZE);
	if (rc)
		return -EFAULT;

	rc = register_rpmsg_driver(&lgr->channel_driver);
	if (!rc)
		lgr->driver_registered = true;
	return rc;
}

static int lgr_open(struct inode *inode, struct file *file)
{
	struct lgr_channel *lgr;

	lgr = kzalloc(sizeof(*lgr), GFP_KERNEL);

	if (!lgr) {
		pr_err("Could not create lgr_channel\n");
		return -ENOMEM;
	}

	/* Initialize struct. */

	mutex_init(&lgr->lock);
	skb_queue_head_init(&lgr->rx_queue);
	init_waitqueue_head(&lgr->rx_waiters);
	init_completion(&lgr->driver_probed);
	INIT_RADIX_TREE(&lgr->guest_epts, GFP_KERNEL);

	file->private_data = lgr;
	return 0;
}

static ssize_t lgr_read(struct file *file, char __user *user, size_t size,loff_t*o)
{
	struct lgr_channel *lgr = file->private_data;
	struct sk_buff *skb;
	ssize_t rc = 0;

	if (unlikely(!lgr->initialized)) {
		rc = wait_for_completion_interruptible_timeout(
				&lgr->driver_probed, timeout_delay);
		if (rc < 0)
			return rc;
		else if (rc == 0)
			return -ETIME;
	}

	if (wait_event_interruptible(lgr->rx_waiters,
			(skb = skb_dequeue(&lgr->rx_queue))))
		return -ERESTARTSYS;

	rc = min(size, skb->len);

	if (copy_to_user(user, skb->data, rc)) {
		dev_err(&lgr->channel->dev, "%s: copy_to_user failed", __func__);
		rc = -EFAULT;
	}

	kfree_skb(skb);
	return rc;
}

static ssize_t lgr_write(struct file *file, const char __user *in,
		     size_t size, loff_t *off)
{
	struct lgr_channel *lgr = file->private_data;
	char message_buf[512];
	ssize_t rc = 0;
	size_t len;
	struct rpmsg_hdr *hdr = (struct rpmsg_hdr *)message_buf;
	struct lgr_endpoint *ept;

	if (unlikely(!lgr->initialized)) {
		/* Wait for our driver to be probed. */
		rc = wait_for_completion_interruptible_timeout(
				&lgr->driver_probed, timeout_delay);
		if (rc < 0)
			return rc;
		else if (rc == 0)
			return -ETIME;
	}

	len = min(sizeof(message_buf), size);

	if (copy_from_user(message_buf, in, len))
		return -EFAULT;

	hdr->len = len - sizeof(*hdr);

	if (mutex_lock_killable(&lgr->lock))
		return -ERESTARTSYS;

	/* Translate inbound name-service messages. */

	if (hdr->dst == RPMSG_NS_ADDR) {
		struct rpmsg_ns_msg *ns = (struct rpmsg_ns_msg *)hdr->data;
		struct lgr_endpoint *ns_ept;

		/* Paranoia. */
		if (hdr->len != sizeof(*ns))
			return -EINVAL;	

		ns_ept = translate_guest_address(lgr, ns->addr);
		if (IS_ERR_OR_NULL(ns_ept))
			return ns_ept ? PTR_ERR(ns_ept) : -EFAULT;
		
		/*
		 * If this is a name-service create, mark this endpoint struct
		 * as being assigned to the name service, so we can clean it up
		 * when the fd gets closed.
		 *
		 * If it's a destroy, clear the mark so we don't send a
		 * duplicate cleanup message.
		 */
		if (!(ns->flags & RPMSG_NS_DESTROY)) {
			ns_ept->is_ns_ept = true;
			memcpy(ept->ns_name, ns->name, RPMSG_NAME_SIZE);
		} else {
			ns_ept->is_ns_ept = false;
		}

		/* Update name service announcement address. */
		ns->addr = ns_ept->host_addr;

	}

	/* Translate source address. */
	ept = translate_guest_address(lgr, hdr->src);
	if (IS_ERR_OR_NULL(ept))
		return ept ? PTR_ERR(ept) : -EFAULT;

	hdr->src = ept->host_addr;

	rc = rpmsg_send_offchannel(lgr->channel, hdr->src, hdr->dst, 
			hdr->data, hdr->len);

	if (rc)
		return rc;
	else
		return len;

}

static long lgr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long rc = 0;
	struct lgr_channel *lgr = file->private_data;

	if (_IOC_TYPE(cmd) != LGR_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > LGR_IOC_MAXNR)
		return -ENOTTY;
	if (cmd != LGR_IOCCONNECT)
		return -ENOTTY;

	if (!lgr->initialized) {
		if (mutex_lock_interruptible(&lgr->lock))
			return -ERESTARTSYS;

		if (!lgr->driver_registered) {
			/*
			 * The very first thing that must happen is the client
			 * writing the name of the channel to connect to.
			 *
			 * We dynamically create an rpmsg_driver that will
			 * respond to that channel name, and register it now.
			 */
			rc = initialize_driver(lgr, (char __user *)arg);
			if (rc)
				goto unlock;
		} else {
			rc = -EISCONN;
			goto unlock;
		}

		mutex_unlock(&lgr->lock);

		/* Wait for our driver to be probed. */
		rc = wait_for_completion_killable_timeout(
				&lgr->driver_probed, timeout_delay);
		if (rc < 0)
			return rc;
		else if (rc == 0)
			return -ETIME;
		else
			return 0;
	}
	
	return -EISCONN;

unlock:
	mutex_unlock(&lgr->lock);
	return rc;
}

static int lgr_close(struct inode *inode, struct file *file)
{
	struct lgr_channel *lgr = file->private_data;
	void *epts[8];
	struct lgr_endpoint *ept;
	struct rpmsg_ns_msg msg;
	unsigned int num_epts = 1, i;
	unsigned long first_idx = 0;

	/* Clean up endpoints. */
	while (num_epts > 0) {
		num_epts = radix_tree_gang_lookup(&lgr->guest_epts, epts,
			first_idx, ARRAY_SIZE(epts));

		for (i = 0; i < num_epts; i++) {
			ept = epts[i];

			/* Send a "destroy" name-service message if
			 * we previously registered ourselves. */
			if (ept->is_ns_ept) {
				memcpy(msg.name, ept->ns_name, RPMSG_NAME_SIZE);
				msg.addr = ept->host_addr;
				msg.flags = RPMSG_NS_DESTROY;

				WARN_ON(rpmsg_sendto(lgr->channel, &msg,
						sizeof(msg), RPMSG_NS_ADDR));
			}


			rpmsg_destroy_ept(ept->ept);
			first_idx = ept->guest_addr;

			radix_tree_delete(&lgr->guest_epts, ept->guest_addr);
			kfree(ept);
		}

	}

	mutex_destroy(&lgr->lock);
	complete_all(&lgr->driver_probed);
	if (lgr->driver_registered)
		unregister_rpmsg_driver(&lgr->channel_driver);

	kfree(lgr);

	return 0;
}

static struct file_operations lguest_rpmsg_fops = {
	.owner		= THIS_MODULE,
	.open		= lgr_open,
	.read		= lgr_read,
	.write		= lgr_write,
	.unlocked_ioctl	= lgr_ioctl,
	.release	= lgr_close,
};

/*
 * This is a textbook example of a "misc" character device.  Populate a "struct
 * miscdevice" and register it with misc_register().
 */
static struct miscdevice lguest_rpmsg_dev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "lguest-rpmsg",
	.fops		= &lguest_rpmsg_fops,
	.nodename	= "rpmsg",
	.mode		= 0600,
};

int __init lguest_rpmsg_device_init(void)
{
	timeout_delay = msecs_to_jiffies(15000);	
	return misc_register(&lguest_rpmsg_dev);
}
module_init(lguest_rpmsg_device_init);

void __exit lguest_rpmsg_device_remove(void)
{
	misc_deregister(&lguest_rpmsg_dev);
}
module_exit(lguest_rpmsg_device_remove);
