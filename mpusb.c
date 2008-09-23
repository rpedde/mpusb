/*
 * This file is based on fsusb_picdem
 *
 * fsusb_picdem is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * fsusb_picdem is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with fsusb_picdem; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

/*
 * portions from usb_pickit by Orion Sky Lawlor, olawlor@acm.org
 */

/*
 * modified to be a blink test for Microchip mchpusb firmware
 * by Ron Pedde <ron@pedde.com>
 */

#include <usb.h> /* libusb header */
#include <unistd.h> /* for geteuid */
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "mpusb.h"

#define CMD_READ_VERSION   0x00
#define CMD_READ_EEPROM    0x01
#define CMD_WRITE_EEPROM   0x02
#define CMD_BOARD_TYPE     0x30
#define CMD_BD_POWER_INFO  0x31
#define CMD_BD_POWER_STATE 0x32
#define CMD_RESET          0xFF

char *board_type[] = {
    "ANY",
    "Power Controller",
    "Generic I2C",
    "Neo-Geo interface"
};

char *processor_type[] = {
    "18F2450",
    "18F2550"
};

const static int mp_vendorID=0x04d8; // Microchip, Inc
const static int mp_productID=0x000c; // PICDEM-FS USB
const static int mp_bootloaderID=0x000b; // when in bootloader mode
const static int mp_configuration=1; /* 1: default config */

const static int mp_interface=0;
const static int mp_endpoint_in=1;
const static int mp_endpoint_out=1;
const static int mp_timeout=1000; /* timeout in ms */

/* Forwards */
int mp_recv_usb(struct mp_handle_t *d,int len, char *dest);
int mp_write_usb(struct mp_handle_t *d, int len, char *src);
int mp_query_info(struct mp_handle_t *d);
struct mp_handle_t *mp_create_handle(struct usb_device *handle);
void mp_destroy_handle(struct mp_handle_t *ph);

#ifdef DEBUG
# define debug_printf(args...) fprintf(stderr, args)
#else
# define debug_printf(args...)
#endif


/*
 * read specific number of bytes from device.
 */
int mp_recv_usb(struct mp_handle_t *d, int len, char *dest) {
    int r;
    int index;

    r=usb_bulk_read(d->phandle, mp_endpoint_out, dest, len, mp_timeout);

    debug_printf("read %i bytes\n", r);
    for(index = 0; index < r; index++) {
        debug_printf("0x%02x ",(unsigned char)dest[index]);
    }
    debug_printf("\n");

    if (r!=len) {
        fprintf(stderr,"Expecting to read %d bytes -- read %d\n",len, r);
        return FALSE;
    }
    return TRUE;
}

/*
 * write specific number of types to device.
 */
int mp_write_usb(struct mp_handle_t *d, int len, char *src) {
    int r;
    int index;

    r = usb_bulk_write(d->phandle, mp_endpoint_in, src, len, mp_timeout);

    debug_printf("wrote %d bytes\n",r);
    for(index = 0; index < r; index++) {
        debug_printf("0x%02x ",src[index]);
    }
    debug_printf("\n");

    if(r != len) {
        fprintf(stderr,"Wanted to write %d bytes -- wrote %d\n",len, r);
        return FALSE;
    }

    return TRUE;
}


/*
 * read eeprom
 */
int mp_read_eeprom(struct mp_handle_t *d, unsigned char addr, unsigned char *retval) {
    char buf[3];
    int result;

    if(!d->has_eeprom)
        return FALSE;

    buf[0] = CMD_READ_EEPROM;
    buf[1] = 1;
    buf[2] = addr;

    debug_printf("executing mp_read_eeprom: %d\n", addr);

    if(mp_write_usb(d,3,buf)) {
        result = mp_recv_usb(d, 2, buf);
        if(result)
            *retval = buf[0];
        return result;
    }

    return FALSE;
}

/*
 * write eeprom
 */
int mp_write_eeprom(struct mp_handle_t *d, unsigned char addr, unsigned char value) {
    char buf[4];

    if(!d->has_eeprom)
        return FALSE;

    buf[0] = CMD_WRITE_EEPROM;
    buf[1] = 2;
    buf[2] = addr;
    buf[3] = value;

    debug_printf("executing mp_write_eeprom: addr %d -> %d\n", addr, value);

    if(mp_write_usb(d,4,buf)) {
        if(mp_recv_usb(d, 4, buf))
            return (buf[0] != 0);
    }

    return FALSE;
}


/*
 * set power for power board
 */
int mp_power_set(struct mp_handle_t *d, int state) {
    char buf[3];

    buf[0] = 0x32;
    buf[1] = 0x1;
    buf[2] = state ? 0x01 : 0x00;

    debug_printf("executing mp_power_set: %d\n",state);

    if(mp_write_usb(d, 3, buf)) {
        // status from op, but always 1 in this case
        return mp_recv_usb(d, 1, buf);
    }

    return FALSE;
}

int mp_query_info(struct mp_handle_t *d) {
    char buf[8];

    // we'll assume we already have the space allocated, and
    // phandle is already assigned

    // First, get version info
    debug_printf("Getting version info\n");
    if((!mp_write_usb(d,2,"\0\0")) || (!mp_recv_usb(d, 2, buf)))
        return FALSE;

    d->fw_major = (int) buf[0];
    d->fw_minor = (int) buf[1];

    // Get board type info
    debug_printf("Getting board info\n");
    if((!mp_write_usb(d,2,"\x30\1")) || (!mp_recv_usb(d, 4, buf)))
        return FALSE;

    d->board_type = (int) buf[0];
    d->serial = (int) buf[1];
    d->processor_type = (int) buf[2];
    d->processor_speed = (int) buf[3];

    d->has_eeprom = 0;
    if(d->processor_type == PROCESSOR_TYPE_2550)
        d->has_eeprom = 1;

    // Get board specific info
    debug_printf("Getting board specific info\n");
    switch(d->board_type) {
    case BOARD_TYPE_POWER:
        if((!mp_write_usb(d,2,"\x31\2")) || (!mp_recv_usb(d, 2, buf)))
            return FALSE;
        d->power.current = buf[0];
        d->power.devices = buf[1];
        break;
    default:
        break;
    }

    return TRUE;
}

/*
 * create a new handle, given a usb device
 */
struct mp_handle_t *mp_create_handle(struct usb_device *device) {
    struct mp_handle_t *pnew;
    usb_dev_handle *phandle;

    if((device->descriptor.idVendor != mp_vendorID) ||
       (device->descriptor.idProduct != mp_productID)) {
        return NULL;
    }

    phandle = usb_open(device);
    if(!phandle)
        return NULL;

    if(usb_set_configuration(phandle, mp_configuration)) {
        usb_close(phandle);
        return NULL;
    }


    if (usb_claim_interface(phandle, mp_interface)) {
        usb_close(phandle);
        return NULL;
    }

    pnew = (struct mp_handle_t *)malloc(sizeof(struct mp_handle_t));
    if(!pnew) {
        usb_release_interface(phandle, mp_interface);
        usb_close(phandle);
        return NULL;
    }

    pnew->phandle = phandle;
    if(!mp_query_info(pnew)) {
        mp_destroy_handle(pnew);
        return NULL;
    }

    return pnew;
}


/*
 * destroy a previously allocated mp handle
 */
void mp_destroy_handle(struct mp_handle_t *ph) {
    usb_release_interface(ph->phandle, mp_interface);
    usb_close(ph->phandle);
    free(ph);
}


/*
 * list all USB devices
 */
int mp_list(void) {
    struct usb_device *device;
    struct usb_bus* bus;
    int found = 0;
    struct mp_handle_t *pmp;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    for (bus=usb_get_busses();bus!=NULL;bus=bus->next) {
        struct usb_device* usb_devices = bus->devices;
        for(device=usb_devices;device!=NULL;device=device->next) {
            pmp = mp_create_handle(device);
            if(pmp) {
                if(!found) {
                    printf("Bus  Serial   Firmware     Proc   Mhz EEPROM  Type\n");
                    printf("----------------------------------------------"
                           "-----------------\n");
                    found = 1;
                }
                printf("%3s    %04d       %d.%02d  %-8s   %-3d   %s  %s\n",
                       device->bus->dirname,
                       pmp->serial,
                       pmp->fw_major,
                       pmp->fw_minor,
                       processor_type[pmp->processor_type],
                       pmp->processor_speed,
                       pmp->has_eeprom ? "Yes" : " No",
                       board_type[pmp->board_type]);

                switch(pmp->board_type) {
                case BOARD_TYPE_POWER:
                    printf("  - %d Amp\n  - %d outlet(s)\n",
                           pmp->power.current,
                           pmp->power.devices);
                    break;
                default:
                    break;
                }

                mp_destroy_handle(pmp);
            }
        }
    }

    return TRUE;
}

/* Find the first USB device with this vendor and product.
 *  Exits on errors, like if the device couldn't be found. -osl
 *
 * This function is heavily based upon Orion Sky Lawlor's
 *  usb_pickit program, which was a very useful reference
 *  for all the USB stuff.  Thanks!
 */
struct mp_handle_t *mp_open(int type, int id) {
    struct usb_device *device;
    struct usb_bus* bus;
    struct mp_handle_t *pmp;

    if (geteuid()!=0) {
        fprintf(stderr,"This program must be run as root.\n");
        exit(1);
    }

#ifdef USB_DEBUG
    usb_debug=4;
#endif

    printf("Locating Monkey Puppet Labs USB device (vendor 0x%04x/product 0x%04x)\n",
           mp_vendorID,mp_productID);

    /* (libusb setup code stolen from John Fremlin's cool "usb-robot") -osl */
    usb_init();
    usb_find_busses();
    usb_find_devices();

    for (bus=usb_get_busses();bus!=NULL;bus=bus->next) {
        struct usb_device* usb_devices = bus->devices;
        for(device=usb_devices;device!=NULL;device=device->next) {
            pmp = mp_create_handle(device);
            if(pmp) {
                if(((pmp->board_type == type) || (type == BOARD_TYPE_ANY)) &&
                   ((id == BOARD_SERIAL_ANY) || (id == pmp->serial))) {
                    return pmp;
                }

                mp_destroy_handle(pmp);
            }
        }
    }
    return NULL;
}
