# ============================================================================
# pts-usb-detach - pts-usb-detach a driver from a usb device.
# 09/05/2007
# ============================================================================

CC	=	gcc
CFLAGS	=	-s -O2 -Wall -W -Werror

# ============================================================================

pts-usb-detach:	pts-usb-detach.c
	$(CC) $(CFLAGS) -o pts-usb-detach pts-usb-detach.c -lusb

# ============================================================================

all:	pts-usb-detach

clean:
	rm -rf a.out *.o pts-usb-detach core foo bar *~

