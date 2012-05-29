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

#ifndef _USB_DRIVERS_H_
#define _USB_DRIVERS_H_

typedef struct usb_drivers_t {
    char *name;
    int interface;
    int configuration;
    int endpoint_in;
    int endpoint_out;

    int (*recognizer)(struct libusb_device_descriptor *);
    int (*write)(struct mp_handle_t *, uint8_t *src, uint8_t slen,
                 uint8_t *dst, uint8_t dlen);
} usb_drivers_t;

#endif /* _USB-DRIVERS_H_ */
