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

#ifndef _USB_TRANSPORT_H_
#define _USB_TRANSPORT_H_

#include "mpusb.h"

int usb_transport_init(struct mp_handle_t *devicelist, void *transport);
int usb_transport_deinit(void);
int usb_transport_destroy(struct mp_handle_t *device);
int usb_transport_write(struct mp_handle_t *device, uint8_t *src, uint8_t slen,
                        uint8_t *dst, uint8_t dlen);

#endif /* _USB_TRANSPORT_H_ */
