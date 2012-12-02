# This creates the demonstration utility "lguest" which runs a Linux guest.
CC   := arm-none-linux-gnueabi-gcc
CFLAGS:=-Wall -Wmissing-declarations -Wmissing-prototypes  -O3 -I../../../include -I../../../arch/arm/include -U_FORTIFY_SOURCE

launcher_objs = lguest.o lguest_setup_tags.o lguest_uboot_header.o


all:lguest 

lguest: $(launcher_objs)
	$(CC)  $(CFLAGS) -o lguest $(launcher_objs)

clean:
	rm -f *.o
	rm -f lguest
