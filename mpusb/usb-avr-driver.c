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
#include "usb-avr-driver.h"
#include "debug.h"

#define VENDOR_RQ_WRITE_BUFFER 0x00
#define VENDOR_RQ_READ_BUFFER  0x01

static struct usb_drivers_t avr_driver = {
    .name = "usb-avr",
    .interface = 0,
    .configuration = 1,
    .endpoint_in = 0x81,
    .endpoint_out = 0x01,
    .recognizer = usb_avr_recognize,
    .write = usb_avr_write
};

/**
 * because the avr vusb uses control transfers, there is a much stronger
 * format for the protocol.  Probably we should enforce the protocol on
 * both types of controllers, but it's too late now.  Instead, we'll
 * just wrap the packet in a control structure and call it a day.
 */
int usb_avr_write(struct mp_handle_t *d, uint8_t *src, uint8_t slen,
                  uint8_t *dst, uint8_t dlen) {
    int cnt;
    int index;

    cnt = libusb_control_transfer(d->phandle,
                                  LIBUSB_REQUEST_TYPE_VENDOR |
                                  LIBUSB_RECIPIENT_DEVICE |
                                  LIBUSB_ENDPOINT_OUT,
                                  VENDOR_RQ_WRITE_BUFFER,
                                  0, 0, src, slen, 5000);
    if(cnt < slen) {
        /* FIXME: better error */
        ERROR("Error on outbound control transfer");
        return FALSE;
    }

    if(dlen) {
        cnt = libusb_control_transfer(d->phandle,
                                      LIBUSB_REQUEST_TYPE_VENDOR |
                                      LIBUSB_RECIPIENT_DEVICE |
                                      LIBUSB_ENDPOINT_IN,
                                      VENDOR_RQ_READ_BUFFER,
                                      0, 0, dst, dlen, 5000);

        if(cnt != dlen) {
            /* FIXME: better error */
            ERROR("Error on read buffer");
            return FALSE;
        }

        SPAM("read %i bytes", cnt);
        for(index = 0; index < cnt; index++) {
            SPAM("0x%02x ",(unsigned char)dst[index]);
        }
    }

    return TRUE;
}

/* see if we can handle a particular descriptor */
int usb_avr_recognize(struct libusb_device_descriptor *pdescriptor) {
    if((pdescriptor->idVendor == 0x16c0) &&
       (pdescriptor->idProduct == 0x05dc))
        return TRUE;

    return FALSE;
}

/* return the fixed driver struct */
usb_drivers_t *usb_avr_driver_table(void) {
    return &avr_driver;
}
