#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <elf.h>
#include <stdlib.h>
#include <string.h>



typedef unsigned long u32;
typedef unsigned short u16;
typedef unsigned char u8;

int parse_uboot_header(int file, u32 *ep_addr, u32 *load_addr);

#define uswap_16(x) \
	((((x) & 0xff00) >> 8) | \
	 (((x) & 0x00ff) << 8))
#define uswap_32(x) \
	((((x) & 0xff000000) >> 24) | \
	 (((x) & 0x00ff0000) >>  8) | \
	 (((x) & 0x0000ff00) <<  8) | \
	 (((x) & 0x000000ff) << 24))


# define cpu_to_le16(x)		(x)
# define cpu_to_le32(x)		(x)
# define cpu_to_le64(x)		(x)
# define le16_to_cpu(x)		(x)
# define le32_to_cpu(x)		(x)
# define le64_to_cpu(x)		(x)
# define cpu_to_be16(x)		uswap_16(x)
# define cpu_to_be32(x)		uswap_32(x)
# define cpu_to_be64(x)		uswap_64(x)
# define be16_to_cpu(x)		uswap_16(x)
# define be32_to_cpu(x)		uswap_32(x)
# define be64_to_cpu(x)		uswap_64(x)


/*
 * The following code is copied here from uboot/include/image.h
 */
#define IH_MAGIC    0x27051956  /* Image Magic Number       */
#define IH_NMLEN        32  /* Image Name Length    */


#define IH_OS_LINUX     5   /* Linux    */
#define IH_ARCH_ARM     2   /* ARM      */
#define IH_TYPE_KERNEL  2   /* OS Kernel Image      */
#define IH_COMP_NONE    0   /*  No   Compression Used   */



/*
 * Legacy format image header,
 * all data in network byte order (aka natural aka bigendian).
 */
typedef struct image_header {
	uint32_t	ih_magic;	/* Image Header Magic Number	*/
	uint32_t	ih_hcrc;	/* Image Header CRC Checksum	*/
	uint32_t	ih_time;	/* Image Creation Timestamp	*/
	uint32_t	ih_size;	/* Image Data Size		*/
	uint32_t	ih_load;	/* Data	 Load  Address		*/
	uint32_t	ih_ep;		/* Entry Point Address		*/
	uint32_t	ih_dcrc;	/* Image Data CRC Checksum	*/
	uint8_t		ih_os;		/* Operating System		*/
	uint8_t		ih_arch;	/* CPU architecture		*/
	uint8_t		ih_type;	/* Image Type			*/
	uint8_t		ih_comp;	/* Compression Type		*/
	uint8_t		ih_name[IH_NMLEN];	/* Image Name		*/
} image_header_t; 





/*we assume that ulgImage is already opened when this function is called.*/
int parse_uboot_header(int file, u32 *ep_addr, u32 *load_addr)
{
	image_header_t uheader;
	int ret = 0;


	ret = lseek(file,0,SEEK_SET);
	read(file,&uheader,sizeof(uheader));


	if((be32_to_cpu(uheader.ih_magic) != IH_MAGIC)) {
		ret = -1;
		printf("Bad image = %x correct=%x\n", uheader.ih_magic, IH_MAGIC); 
		goto out;
	}
	if(uheader.ih_os != IH_OS_LINUX) {
		ret = -1;
		printf("This is not Linux\n");
		goto out;
	}
	if(uheader.ih_arch != IH_ARCH_ARM) {
		ret = -1;
		printf("Architecture is not ARM.\n");
		goto out;
	}
	if(uheader.ih_type != IH_TYPE_KERNEL) {
		ret = -1;
		printf("Type of this image is not Kernel.\n");
		goto out;
	}
	if(uheader.ih_comp != IH_COMP_NONE){
		ret = -1;
		printf("Lguest launcher Cannot deal with the compressed image at present.\n");
		goto out;
	}
	/* 
	 * return the address where the image should be load and 
	 * entry pointer of kernel.	 
	 */
	*load_addr = be32_to_cpu(uheader.ih_load);
	*ep_addr = be32_to_cpu(uheader.ih_ep);
out:
	return ret;
}


