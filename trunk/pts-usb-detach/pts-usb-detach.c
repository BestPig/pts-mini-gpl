/*
 * $RCSfile: detach.c,v $  $Revision: 1.1 $  $Name:  $
 * $Id: detach.c,v 1.1 2006/02/10 01:31:36 bpaauwe Exp $
 * $Author: bpaauwe $
 * $Date: 2006/02/10 01:31:36 $
 * modified by pts@fazekas.hu at Tue Oct  6 08:51:32 CEST 2009
 * ----------------------------------------------------------------------------
 *
 *  Copyright (c) Bob Paauwe (2006)
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * ----------------------------------------------------------------------------
 *
 *  Description:
 *
 *  This is a simple program that uses libusb to detach the Insteon
 *  PLC from the kernel HID driver.
 */

/*
** Now it's a more complex program to detach various drivers from
** various devices (passed as cli parameters). I'm still working on
** the option to detach either the first or second driver/device
** found. This will allow for those who have two devices.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usb.h>

#include <stdlib.h>
#include <errno.h>

#include <ctype.h>

#define IPLC_VENDOR     0x10bf
#define IPLC_PRODUCT    0x0004

struct myusbdevice {
  int vendor;
  int prodid;
  char *product;
};

#define TEN ':'
#define HEX_A 'g'

int
htoi(char *str) {
  int i;
  u_char c;
  int len;
  int value = 0;

  len = strlen(str);

  for(i = 0; i < len; i++) {
    c = *str;

    if(isxdigit(c)) {
      value = value << 4;

      if(c < TEN) {
	value += c - '0';
      } else if(c < HEX_A) {
	// Lowercase hex values
	value += c - 'W';	// hex a = 97 - 'a' = 0 + 10 = 10 = 'a' - 'W' (57)
      } else {
	// Uppercase hex values
	value += c - '7';	// hex A = 65 - 'A' = 0 + 10 = 10 = 'A' - '7' (55)
      }
    } else {
      return(-1);
    }
    str++;
  }
  return(value);
}

// argv[0]
// argv[1]
// argv[2]
void
options(int argc, char **argv, struct myusbdevice *device) {
  int i;

  printf("Count: %d\n", argc);

  for(i = 0; i < argc; i++) {
    printf("Argv[%d] = %s\n", i, argv[i]);
  }
  if (argc != 3) abort();
  if (1 != sscanf(argv[1], "%x", &device->vendor) ||
      1 != sscanf(argv[2], "%x", &device->prodid)) abort();

}

/*
 * locate_iplc
 *
 * Scan the USB bus for a device matching the Insteon USB PLC. 
 */
usb_dev_handle *locate_iplc(struct myusbdevice *device)
{
	struct usb_bus *busses, *bus;
	struct usb_device *dev;
	struct usb_dev_handle *dev_handle = NULL;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	busses = usb_get_busses();

	for (bus = busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
		  if (!dev_handle && (dev->descriptor.idVendor == device->vendor) &&
		      (dev->descriptor.idProduct == device->prodid)) {
		    /* usb_info(dev); */
		    dev_handle = usb_open(dev);
		  }
		}
	}
	return dev_handle;
}

int
main(int argc, char **argv)
{
	struct usb_dev_handle *iplc = NULL;
	struct myusbdevice *device;

	usb_set_debug(0);

	if(!(device = malloc(sizeof(struct myusbdevice)))) {
	  printf("Can't malloc USB device structure, errno %d\n", errno);
	  exit(errno);
	}
	// This is needed in case we want to detach something else
	options(argc, argv, device);
        printf("finding USB device with vendor=0x%04x prodid=0x%04x\n",
               device->vendor, device->prodid);
	/*
	 * Find the USB PLC and clain the interface.  This will also try
	 * to detach any kernel driver that has previously claimed the
	 * device.  Specificlly, the Linux HID driver will claim the PLC
	 * when it's attached since the PLC has a HID class.
	 */
	if ((iplc = locate_iplc(device)) != NULL) {
                char driver[256], interface;
                printf("device found\n");
  	        /* always 1; printf("interfaces=%d\n", usb_device(iplc)->config->interface->num_altsetting); */
                for (interface = 0;
                     0 == usb_get_driver_np(iplc, interface, driver, sizeof(driver));
                     ++interface) {
                  printf("interface %d driver is %s\n", interface, driver);
                  if (0 == strcmp(driver, "usbhid")) {
                    if (usb_detach_kernel_driver_np(iplc, interface) != 0) {
		      printf("Error detaching kernel driver: %s\n", usb_strerror());
		      exit(1);
                    }
                    printf("interface %d driver detached\n", interface);
                  }
		}
		if (0 == interface) {
		  printf("No interfaces found (maybe already detached?).\n");
		  exit(1);
                }
		printf("no more interfaces.\n");
		usb_close(iplc);
	} else {
		printf("Unable to find device 0x%04x:0x%04x (maybe not connected?)\n", device->vendor, device->prodid);
		exit(1);
	}
	if(device) {
	  free(device);
	}
	exit(0);
}
