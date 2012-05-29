/*
 * Copyright (C) 2012 Ron Pedde <ron@pedde.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpusb.h"
#include "usb-drivers.h"
#include "usb-pic-driver.h"
#include "debug.h"

#define PIC_TIMEOUT 1000

static struct usb_drivers_t pic_driver = {
    .name = "usb-pic",
    .interface = 0,
    .configuration = 1,
    .endpoint_in = 0x81,
    .endpoint_out = 0x01,
    .recognizer = usb_pic_recognize,
    .write = usb_pic_write
};

int pic_read_bytes(struct mp_handle_t *d, uint8_t len, uint8_t *dest) {
    int r;
    int index;

    if(libusb_bulk_transfer(d->phandle, pic_driver.endpoint_in,
                            (unsigned char *)dest, len, &r, PIC_TIMEOUT) != 0) {
        ERROR("Error receiving data");
        return FALSE;
    }

    SPAM("read %i bytes", r);
    for(index = 0; index < r; index++) {
        SPAM("0x%02x ",(unsigned char)dest[index]);
    }

    if (r!=len) {
        ERROR("Expecting to read %d bytes -- read %d\n",len, r);
        // FIXME:
        //        fprintf(stderr,"%s",usb_strerror());
        return FALSE;
    }
    return TRUE;
}

/*
 * write specific number of types to device.
 */
int pic_write_bytes(struct mp_handle_t *d, uint8_t len, uint8_t *src) {
    int r;
    int index;
    int err;

    SPAM("Writing %d bytes", len);

    if((err=libusb_bulk_transfer(d->phandle, pic_driver.endpoint_out,
                            (unsigned char *)src,
                                 len, &r, PIC_TIMEOUT))) {
        INFO("Error writing data: %s", libusb_error_name(err));
        return FALSE;
    }

    SPAM("wrote %d bytes",r);
    for(index = 0; index < r; index++) {
        SPAM("0x%02x ",(unsigned char)src[index]);
    }

    if(r != len) {
        ERROR("Wanted to write %d bytes -- wrote %d\n",len, r);
        // FIXME:
        //        fprintf(stderr,"%s",usb_strerror());
        return FALSE;
    }

    return TRUE;
}

int pic_write_with_response(struct mp_handle_t *d, uint8_t len, uint8_t *src,
                            uint8_t dlen, uint8_t *dst) {
    if(pic_write_bytes(d, len, src)) {
        return pic_read_bytes(d, dlen, dst);
    }
    return FALSE;
}

/* write a command with response */
int usb_pic_write(struct mp_handle_t *d, uint8_t *src, uint8_t slen,
                  uint8_t *dst, uint8_t dlen) {
    return pic_write_with_response(d, slen, src, dlen, dst);
}

/* see if we can handle a particular descriptor */
int usb_pic_recognize(struct libusb_device_descriptor *pdescriptor) {
    if((pdescriptor->idVendor == 0x04d8) &&
       (pdescriptor->idProduct == 0x000c))
        return TRUE;

    return FALSE;
}

/* return the fixed driver struct */
usb_drivers_t *usb_pic_driver_table(void) {
    return &pic_driver;
}
