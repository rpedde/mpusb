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

#include <libusb.h>
#include <unistd.h> /* for geteuid */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <errno.h>
#include "main.h"
#include "mpusb.h"
#include "debug.h"

#include "usb-transport.h"

#define VENDOR_RQ_WRITE_BUFFER 0x00
#define VENDOR_RQ_READ_BUFFER  0x01

#define MAX_INTERRUPT_TRANSFER 20

char *board_type[] = {
    "ANY",
    "Power Controller",
    "Generic I2C",
    "Unknown"
};

char *processor_type[] = {
    "18F2450",
    "18F2550",
    "ATmega168",
    "Atmega88",
    "Unknown",
};

char *i2c_type[] = {
    "18F690 Boot Loader",
    "HD44780 LCD Panel",
    "Servo Controller",
    "Generic 8-bit IO",
    "Unknown"
};

static struct libusb_context *mp_ctx;
static struct mp_handle_t devicelist;

const static int mp_vendorID=0x04d8; // Microchip, Inc
const static int mp_productID=0x000c; // PICDEM-FS USB

const static int mp_vusb_vendorID=0x16c0;
const static int mp_vusb_productID=0x05dc;

const static int mp_bootloaderID=0x000b; // when in bootloader mode
const static int mp_configuration=1; /* 1: default config */

const static int mp_interface=0;
const static int mp_endpoint_in=0x81;
const static int mp_endpoint_out=0x01;
const static int mp_timeout=1000; /* timeout in ms */

static int mp_i2c_min = I2C_LOW;
static int mp_i2c_max = I2C_HIGH;

static pthread_mutex_t mp_async_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t mp_async_tid=NULL;

typedef struct transport_t {
    char *name;
    int (*init)(struct mp_handle_t *devicelist, void *transport);
    int (*deinit)(void);
    int (*destroy)(struct mp_handle_t *device);
    int (*write)(struct mp_handle_t *device, uint8_t *src, uint8_t slen,
                 uint8_t *dst, uint8_t dlen);
} transport_t;

struct transport_t transport_table[] = {
    { .name = "usb",
      .init = usb_transport_init,
      .deinit = usb_transport_deinit,
      .destroy = usb_transport_destroy,
      .write = usb_transport_write,
    },
    { .name = NULL }
};

/* Forwards */
void mp_set_debug(int value);
int mp_recv_usb(struct mp_handle_t *d,int len, char *dest);
int mp_write_usb(struct mp_handle_t *d, int len, char *src);
int mp_write_usb_with_response(struct mp_handle_t *d, int len, char *src, int dlen, char *dst);

int mp_query_info(struct mp_handle_t *d);
void mp_release_handle(struct mp_handle_t *ph);
void mp_destroy_handle(struct mp_handle_t *ph);
int mp_read_eeprom(struct mp_handle_t *d, unsigned char addr, unsigned char *retval);
int mp_write_eeprom(struct mp_handle_t *d, unsigned char addr, unsigned char value);
int mp_i2c_read(struct mp_handle_t *d, unsigned char dev, unsigned char addr, unsigned char len, unsigned char *data);
int mp_i2c_write(struct mp_handle_t *d, unsigned char dev, unsigned char addr, unsigned char len, unsigned char *data);

void mp_set_debug(int value) {
    debug_level(value);
}


char *mp_i2c_type(uint8_t id) {
    if(id > I2C_UNKNOWN)
        return i2c_type[I2C_UNKNOWN];
    return i2c_type[id];
}

/*
 * polling callback for libusb
 */
void *mp_async_proc(void *arg) {
    struct timeval tv;

    while(1) {
        tv.tv_sec=5;
        tv.tv_usec=0;
        libusb_handle_events_timeout(mp_ctx, &tv);
    }

    return NULL;
}

/* FIXME: abstract to driver */
void mp_irq_callback(struct libusb_transfer *xfer) {
    int err;
    struct mp_handle_t *phandle;

    phandle = (struct mp_handle_t*)xfer->user_data;

    if(xfer->status == LIBUSB_TRANSFER_COMPLETED) {
        DEBUG("Length: %d", xfer->actual_length);
    }

    if(xfer->actual_length) {
        phandle->cb(xfer->buffer[0], xfer->actual_length - 1,
                    (char*)&xfer->buffer[1]);
    }

    if((err = libusb_submit_transfer(xfer)) != 0) {
        ERROR("Error submitting transfer: %d", err);
    }
}


/*
 * Set up for async callbacks.
 */
int mp_async_callback(struct mp_handle_t *d, callback_function cb) {
    struct libusb_transfer *xfer;
    unsigned char *buffer;
    int err;

    buffer=(unsigned char *)malloc(MAX_INTERRUPT_TRANSFER);
    if(!buffer)
        return FALSE;

    /* if we havne't already set up a async transfers, then we'll
     * go ahead and spin off a poller thread.  Otherwise, we'll
     * hang this device off the callback chain and register an
     * interrupt listener.
     */
    pthread_mutex_lock(&mp_async_mutex);
    if(!mp_async_tid) {
        if(pthread_create(&mp_async_tid, NULL, mp_async_proc, NULL) < 0) {
            ERROR("Error creating pthread: %s", strerror(errno));
            return FALSE;
        }
    }

    if(d->cb) {
        ERROR("Device already has callback registered");
        pthread_mutex_unlock(&mp_async_mutex);
        return FALSE;
    }

    d->cb = cb;
    pthread_mutex_unlock(&mp_async_mutex);


    /* we've got a poller, now let's start listening for async
       events on the device passed */

    xfer = libusb_alloc_transfer(0);
    if(!xfer) {
        ERROR("Can't alloc transfer buffer");
        return FALSE;
    }

    libusb_fill_interrupt_transfer(xfer, d->phandle, 0x81, buffer,
                                   MAX_INTERRUPT_TRANSFER,
                                   mp_irq_callback,
                                   (void*)d, 0);

    if((err = libusb_submit_transfer(xfer)) != 0) {
        ERROR("Error submitting transfer: %d", err);
        return FALSE;
    }

    return TRUE;
}

/*
 * read from i2c device
 */
int mp_i2c_read(struct mp_handle_t *d, unsigned char dev, unsigned char addr, unsigned char len, unsigned char *data) {
    uint8_t *buf;
    int result = FALSE;
    int retval;
    transport_t *ptransport = d->transport_info;

    if(d->board_id != BOARD_TYPE_I2C) {
        return FALSE;
    }

    buf = malloc(len + 5); // make sure we have enough for the headers
    if(!buf) {
        perror("malloc");
        return FALSE;
    }

    buf[0] = CMD_I2C_READ;
    buf[1] = 2;
    buf[2] = dev;
    buf[3] = addr;
    buf[4] = len;

    DEBUG("executing mp_i2c_read: dev 0x%02x, addr 0x%02x, len 0x%02x", dev, addr, len);

    if((result = ptransport->write(d, buf, 5, buf, len + 1))) {
        retval = buf[0];
        memcpy(data, &buf[1],len);
        free(buf);
        return retval;
    }

    free(buf);
    return result;
}

/*
 * write to an i2c device
 */
int mp_i2c_write(struct mp_handle_t *d, uint8_t dev, uint8_t addr, uint8_t len, uint8_t *data) {
    uint8_t *buf;
    int result = FALSE;
    int retval;
    transport_t *ptransport = d->transport_info;

    if(d->board_id != BOARD_TYPE_I2C) {
        return FALSE;
    }

    buf = malloc(len + 5); // make sure we have enough for the headers
    if(!buf) {
        perror("malloc");
        return FALSE;
    }

    buf[0] = CMD_I2C_WRITE;
    buf[1] = 2 + len;
    buf[2] = dev;
    buf[3] = addr;

    memcpy(&buf[4],data,len);

    DEBUG("executing mp_i2c_write: dev 0x%02x, addr 0x%02x, len 0x%02x", dev, addr, len);

    if((result = ptransport->write(d, buf, len + 4, buf, 2))) {
        retval=buf[0];
        data[0] = buf[1];
        free(buf);
        return retval;
    }

    free(buf);
    return result;
}


/*
 * read eeprom
 */
int mp_read_eeprom(struct mp_handle_t *d, uint8_t addr, uint8_t *retval) {
    uint8_t buf[3];
    int result;
    transport_t *ptransport = d->transport_info;

    if(!d->has_eeprom)
        return FALSE;

    buf[0] = CMD_READ_EEDATA;
    buf[1] = 1;
    buf[2] = addr;

    DEBUG("executing mp_read_eeprom: %d", addr);

    if((result = ptransport->write(d, buf, 3, buf, 2))) {
        *retval = buf[0];
        return result;
    }

    return FALSE;
}

/*
 * write eeprom
 */
int mp_write_eeprom(struct mp_handle_t *d, uint8_t addr, uint8_t value) {
    uint8_t buf[4];
    transport_t *ptransport = d->transport_info;

    if(!d->has_eeprom)
        return FALSE;

    buf[0] = CMD_WRITE_EEDATA;
    buf[1] = 2;
    buf[2] = addr;
    buf[3] = value;

    DEBUG("executing mp_write_eeprom: addr %d -> %d", addr, value);

    if(ptransport->write(d, buf, 4, buf, 4)) {
        return (buf[0] != 0);
    }

    return FALSE;
}


/*
 * set power for power board
 */
int mp_power_set(struct mp_handle_t *d, uint8_t state) {
    uint8_t buf[3];
    transport_t *ptransport = d->transport_info;

    buf[0] = 0x32;
    buf[1] = 0x1;
    buf[2] = state ? 0x01 : 0x00;

    DEBUG("executing mp_power_set: %d",state);

    return ptransport->write(d, buf, 3, buf, 1);
}

/**
 * set the default low point on the i2c bus query
 */
int mp_i2c_default_min(int min) {
    mp_i2c_min = min;
    return TRUE;
}

/**
 * set the default high point on the i2c bus query
 */
int mp_i2c_default_max(int max) {
    mp_i2c_max = max;
    return TRUE;
}

int mp_query_info(struct mp_handle_t *d) {
    uint8_t buf[8];
    int index;
    int result;
    struct mp_i2c_handle_t *pi2c;
    transport_t *ptransport = d->transport_info;

    DEBUG("Querying device %s on transport %s", d->device_path, ptransport->name);
    DEBUG("Getting version info");
    if(!ptransport->write(d, (uint8_t*)"\0\0", 2, buf, 2))
        return FALSE;

    d->fw_major = (int) buf[0];
    d->fw_minor = (int) buf[1];

    // Get board type info
    DEBUG("Getting board info");
    if(!ptransport->write(d, (uint8_t *)"\x30\1", 2, buf, 4))
        return FALSE;

    d->board_id = (int) buf[0];
    d->board_type = board_type[BOARD_TYPE_UNKNOWN];

    if(d->board_id < BOARD_TYPE_UNKNOWN)
        d->board_type = board_type[d->board_id];

    d->serial = (unsigned int) buf[1];
    d->processor_id = (unsigned int) buf[2];
    d->processor_type = processor_type[PROCESSOR_TYPE_UNKNOWN];
    if(d->processor_id < PROCESSOR_TYPE_UNKNOWN)
        d->processor_type = processor_type[d->processor_id];

    d->processor_speed = (unsigned int) buf[3];

    d->has_eeprom = 0;
    if(d->processor_id == PROCESSOR_TYPE_2550)
        d->has_eeprom = 1;

    d->i2c_list.pnext = NULL;
    d->i2c_devices = 0;

    // Get board specific info
    DEBUG("Getting board specific info");
    switch(d->board_id) {
    case BOARD_TYPE_POWER:
        if(!ptransport->write(d, (uint8_t *)"\x31\2", 2, buf, 2))
            return FALSE;
        d->power.current = buf[0];
        d->power.devices = buf[1];
        break;
    case BOARD_TYPE_I2C:
        for(index = mp_i2c_max; index >= mp_i2c_min; index--) {
            if((result = mp_i2c_read(d, index, 0, 1, (unsigned char *)&buf[0]))) {
                /* we found an i2c device */
                d->i2c_devices++;
                pi2c = (struct mp_i2c_handle_t*)malloc(sizeof(struct mp_i2c_handle_t));
                if(!pi2c) {
                    perror("malloc");
                    exit(1);
                }
                memset(pi2c,0,sizeof(struct mp_i2c_handle_t));

                pi2c->device = index;
                if((unsigned char)buf[0] == 0xAE) {
                    pi2c->mpusb = 1;
                    /* see what kind... */
                    if((result = mp_i2c_read(d, index, 1, 1, (unsigned char *)&buf[0]))) {
                        pi2c->i2c_id = buf[0];
                    }
                }

                pi2c->pnext = d->i2c_list.pnext;
                d->i2c_list.pnext = pi2c;
            }
        }
    default:
        break;
    }

    return TRUE;
}

/*
 * destroy a previously allocated mp handle
 */
void mp_destroy_handle(struct mp_handle_t *ph) {

    libusb_reset_device(ph->phandle);
    libusb_release_interface(ph->phandle, mp_interface);
    libusb_close(ph->phandle);

    /* FIXME: Walk the i2c devices */
    free(ph);
}

/*
 * release a handle
 */
void mp_release_handle(struct mp_handle_t *ph) {
    libusb_reset_device(ph->phandle);
    libusb_release_interface(ph->phandle, mp_interface);
}

/*
 * list all USB devices
 */
int mp_list(void) {
    struct mp_i2c_handle_t *pi2c;
    int found = 0;
    struct mp_handle_t *pmp;

    pmp = devicelist.pnext;

    while(pmp) {
        if(!found) {
            printf("\n%-8s %-10s %-10s %-6s %-7s %-5s\n","Serial", "Firmware", "Proc", "MHz", "EEPROM", "Type");
            printf("----------------------------------------------"
                   "-----------------\n");
            found = 1;
        }
        printf("%04d    %2d.%02d       %-8s   %-3d    %-3s     %s\n",
               pmp->serial,
               pmp->fw_major,
               pmp->fw_minor,
               pmp->processor_type,
               pmp->processor_speed,
               pmp->has_eeprom ? "Yes" : "No",
               pmp->board_type);

        switch(pmp->board_id) {
        case BOARD_TYPE_POWER:
            printf("  - %d Amp\n  - %d outlet(s)\n",
                   pmp->power.current,
                   pmp->power.devices);
            break;
        case BOARD_TYPE_I2C:
            pi2c = pmp->i2c_list.pnext;
            while(pi2c) {
                printf(" - I2C Device %02d: %s\n",
                       pi2c->device, pi2c->mpusb ? mp_i2c_type(pi2c->i2c_id) :
                       "Non-16F690 Device");
                pi2c = pi2c->pnext;
            }
            break;

        default:
            break;
        }

        pmp = pmp->pnext;
    }

    return TRUE;
}

void mp_close(struct mp_handle_t *d) {
    mp_release_handle(d);
}

struct mp_handle_t *mp_open(uint8_t type, uint8_t id) {
    struct mp_handle_t *pmp;

    pmp = devicelist.pnext;
    while(pmp) {
        if(((pmp->board_id == type) || (type == BOARD_TYPE_ANY)) &&
           ((id == BOARD_SERIAL_ANY) || (id == pmp->serial))) {

            if(libusb_set_configuration(pmp->phandle, mp_configuration) != 0) {
                DEBUG("Error in set_configuration");
                return NULL;
            }

            if (libusb_claim_interface(pmp->phandle, mp_interface) != 0) {
                DEBUG("Error in claim_interface");
                return NULL;
            }

            return pmp;
        }
        pmp = pmp->pnext;
    }
    return NULL;
}

struct mp_handle_t *mp_devicelist(void) {
    return devicelist.pnext;
}

int mp_init(void) {
    struct transport_t *current = transport_table;
    struct mp_handle_t *pdevice;

    devicelist.pnext = NULL;

    while(current->name) {
        DEBUG("Initializing transport %s", current->name);
        current->init(&devicelist, current);
        current++;
    }

    pdevice=devicelist.pnext;
    while(pdevice) {
        if(!pdevice->queried) {
            DEBUG("Forcing a query on device %s", pdevice->device_path);
            mp_query_info(pdevice);
        }
        pdevice = pdevice->pnext;
    }

    return 0;
}

/**
 * release everything
 */
void mp_deinit(void) {
    struct mp_handle_t *current, *next;

    /* walk through all the devices and close them */
    current = devicelist.pnext;
    while(current) {
        next = current->pnext;
        ((transport_t*)(current->transport_info))->destroy(current);
        current = next;
    }

    struct transport_t *tcurrent = transport_table;

    while(tcurrent->name) {
        DEBUG("Deinitializing transport %s", tcurrent->name);
        tcurrent->deinit();
        tcurrent++;
    }
}
