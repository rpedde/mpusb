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
#include "debug.h"
#include "usb-drivers.h"
#include "usb-pic-driver.h"
#include "usb-avr-driver.h"

static struct libusb_context *mp_ctx = NULL;
static char *transport_name="usb";

usb_drivers_t *driver_table[3];
static int usb_drivers = 0;

typedef struct usb_driverinfo_t {
    uint8_t bus;
    uint8_t address;
    usb_drivers_t *driver;
} usb_driverinfo_t;

struct mp_handle_t *usb_create_stub(struct libusb_device *device,
                                    void *ptransport,
                                    usb_drivers_t *driver) {
    struct mp_handle_t *pnew;
    struct usb_driverinfo_t *pdriver = NULL;
    libusb_device_handle *phandle = NULL;
    int err;

    if((err = libusb_open(device, &phandle))) {
        DEBUG("Could not open device: %s", libusb_error_name(err));
        return NULL;
    }

    if((err = libusb_set_configuration(phandle, driver->configuration))) {
        DEBUG("Error in set_configuration: %s", libusb_error_name(err));
        libusb_close(phandle);
        return NULL;
    }

    if((err = libusb_claim_interface(phandle, driver->interface))) {
        DEBUG("Error in claim_interface: %s", libusb_error_name(err));
        libusb_close(phandle);
        return NULL;
    }

    pnew = (struct mp_handle_t *)malloc(sizeof(struct mp_handle_t));
    pdriver = (usb_driverinfo_t *)malloc(sizeof(usb_driverinfo_t));

    if((!pnew)||(!pdriver)) {
        ERROR("Malloc");
        libusb_release_interface(phandle, driver->interface);
        libusb_close(phandle);
        if(pnew) free(pnew);
        if(pdriver) free(pdriver);
        return NULL;
    }

    pdriver->bus = libusb_get_bus_number(device);
    pdriver->address = libusb_get_device_address(device);
    pdriver->driver = driver;

    pnew->driver_info = pdriver;
    pnew->transport_info = ptransport;
    pnew->queried = 0;
    asprintf(&pnew->device_path, "%s:%d:%d", transport_name,
             pdriver->bus, pdriver->address);

    pnew->phandle = phandle;
    pnew->handle_locked = TRUE;

    return pnew;
}

/**
 * walk the device table and see if we can find a
 * device with the specified bus and address
 */
int usb_find_device(struct mp_handle_t *devicelist, int bus, int address) {
    char path[40];
    struct mp_handle_t *current = devicelist->pnext;

    snprintf(path, sizeof(path), "%s:%d:%d", transport_name, bus, address);
    while(current) {
        if(!strcmp(path, current->device_path))
            return TRUE;
        current=current->pnext;
    }

    return FALSE;
}

/**
 * walk the libusb device list and see if there
 * are any devices that have not yet been found.
 *
 * it adds stubbed device entries for each device
 * it finds.
 */
int usb_scan_changes(struct mp_handle_t *devicelist, void *ptransport) {
    libusb_device **list;
    ssize_t cnt;
    ssize_t i;
    struct libusb_device_descriptor descriptor;
    usb_drivers_t *current;
    int err;
    libusb_device_handle *phandle;
    uint8_t bus;
    uint8_t address;
    struct mp_handle_t *stub;
    int driver;

    DEBUG("Scanning for usb device changes");

    cnt = libusb_get_device_list(mp_ctx, &list);
    if(cnt < 0) {
        DEBUG("No usb devices found");
        return TRUE;
    }

    DEBUG("Found %d devices", cnt + 1);

    for(i=0; i < cnt; i++) {
        DEBUG("Checking device %d", i);
        libusb_device *device = list[i];

        if((err = libusb_get_device_descriptor(device, &descriptor))) {
            DEBUG("Error getting device descriptor: %s", libusb_error_name(err));
            continue;
        }

        for(driver = 0; driver < usb_drivers; driver++) {
            current = driver_table[driver];
            if(current->recognizer(&descriptor)) {
                if((err = libusb_open(device, &phandle))) {
                    ERROR("Error opening device: %s", libusb_error_name(err));
                } else {
                    bus = libusb_get_bus_number(device);
                    address = libusb_get_device_address(device);
                    DEBUG("Driver %s recognized device at %d:%d",
                          current->name, bus, address);

                    /* see if that device is already in the table */
                    if(!usb_find_device(devicelist, bus, address)) {
                        stub = usb_create_stub(device, ptransport, current);
                        if(stub) {
                            DEBUG("Adding new device: %s", stub->device_path);
                            stub->pnext = devicelist->pnext;
                            devicelist->pnext = stub;
                        }
                    }
                }
            }
        }
    }

    libusb_free_device_list(list, 1);
    DEBUG("USB device scan done");

    return TRUE;
}

/**
 * initialize the usb bus, and fill in the devicelist
 * with any discovered devices
 */
int usb_transport_init(struct mp_handle_t *devicelist, void *ptransport) {
    int err;

#ifdef MUST_BE_ROOT
    if (geteuid() != 0) {
        ERROR("This program must be run as root for usb discovery");
        return FALSE;
    }
#endif
    memset(driver_table, 0, sizeof(driver_table));
    driver_table[0] = usb_pic_driver_table();
    driver_table[1] = usb_avr_driver_table();
    usb_drivers = 2;

    if ((err = libusb_init(&mp_ctx))) {
        ERROR("Error initializing libusb: %s", libusb_error_name(err));
        return FALSE;
    }

    libusb_set_debug(mp_ctx, 3);

    return usb_scan_changes(devicelist, ptransport);
}

/* call the proper write dispatcher */
int usb_transport_write(struct mp_handle_t *device, uint8_t *src, uint8_t slen,
                        uint8_t *dst, uint8_t dlen) {

    usb_drivers_t *pdriver = ((usb_driverinfo_t *)(device->driver_info))->driver;
    SPAM("Dispatching write of %d bytes to %s", slen, pdriver->name);
    return pdriver->write(device, src, slen, dst, dlen);
}

/* tear down a single device */
int usb_transport_destroy(struct mp_handle_t *device) {
    free(device);
    return TRUE;
}

int usb_transport_deinit(void) {
    libusb_exit(mp_ctx);
    return TRUE;
}
