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
#include <stdarg.h>
#include "main.h"
#include "mpusb.h"

#define CMD_READ_VERSION   0x00
#define CMD_READ_EEPROM    0x01
#define CMD_WRITE_EEPROM   0x02
#define CMD_BOARD_TYPE     0x30
#define CMD_BD_POWER_INFO  0x31
#define CMD_BD_POWER_STATE 0x32
#define CMD_I2C_READ       0x40
#define CMD_I2C_WRITE      0x41
#define CMD_RESET          0xFF

#define VENDOR_RQ_WRITE_BUFFER 0x00
#define VENDOR_RQ_READ_BUFFER  0x01

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
    "Unknown",
};

char *i2c_type[] = {
    "18F690 Boot Loader",
    "HD44780 LCD Panel",
    "Servo Controller",
    "Generic 8-bit IO",
    "Unknown"
};

static struct mp_handle_t devicelist;

const static int mp_vendorID=0x04d8; // Microchip, Inc
const static int mp_productID=0x000c; // PICDEM-FS USB

const static int mp_vusb_vendorID=0x16c0;
const static int mp_vusb_productID=0x05dc;

const static int mp_bootloaderID=0x000b; // when in bootloader mode
const static int mp_configuration=1; /* 1: default config */

const static int mp_interface=0;
const static int mp_endpoint_in=1;
const static int mp_endpoint_out=1;
const static int mp_timeout=1000; /* timeout in ms */

static int mp_i2c_min = I2C_LOW;
static int mp_i2c_max = I2C_HIGH;

static int mp_debug = 0;

/* Forwards */
void mp_set_debug(int value);
int mp_recv_usb(struct mp_handle_t *d,int len, char *dest);
int mp_write_usb(struct mp_handle_t *d, int len, char *src);
int mp_write_usb_with_response(struct mp_handle_t *d, int len, char *src, int dlen, char *dst);

int mp_query_info(struct mp_handle_t *d);
struct mp_handle_t *mp_create_handle(struct usb_device *handle);
void mp_release_handle(struct mp_handle_t *ph);
void mp_destroy_handle(struct mp_handle_t *ph);
int mp_read_eeprom(struct mp_handle_t *d, unsigned char addr, unsigned char *retval);
int mp_write_eeprom(struct mp_handle_t *d, unsigned char addr, unsigned char value);
int mp_i2c_read(struct mp_handle_t *d, unsigned char dev, unsigned char addr, unsigned char len, unsigned char *data);
int mp_i2c_write(struct mp_handle_t *d, unsigned char dev, unsigned char addr, unsigned char len, unsigned char *data);

#define debug_printf(args...) mp_log(args)

void mp_set_debug(int value) {
    mp_debug=value;
}

void mp_log(char *fmt, ...) {
    char errbuf[4096];
    va_list ap;

    if(!mp_debug)
        return;

    va_start(ap, fmt);
    vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
    va_end(ap);

    fprintf(stderr,"MPUSB: %s\n", errbuf);
}

/*
 * read specific number of bytes from device.
 */
int mp_recv_usb(struct mp_handle_t *d, int len, char *dest) {
    int r;
    int index;

    r=usb_bulk_read(d->phandle, mp_endpoint_out, dest, len, mp_timeout);

    debug_printf("read %i bytes", r);
    for(index = 0; index < r; index++) {
        debug_printf("0x%02x ",(unsigned char)dest[index]);
    }
    debug_printf("\n");

    if (r!=len) {
        fprintf(stderr,"Expecting to read %d bytes -- read %d\n",len, r);
        fprintf(stderr,"%s",usb_strerror());
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

    debug_printf("wrote %d bytes",r);
    for(index = 0; index < r; index++) {
        debug_printf("0x%02x ",(unsigned char)src[index]);
    }

    if(r != len) {
        fprintf(stderr,"Wanted to write %d bytes -- wrote %d\n",len, r);
        fprintf(stderr,"%s",usb_strerror());
        return FALSE;
    }

    return TRUE;
}

/**
 * because the avr vusb uses control transfers, there is a much stronger
 * format for the protocol.  Probably we should enforce the protocol on
 * both types of controllers, but it's too late now.  Instead, we'll
 * just wrap the packet in a control structure and call it a day.
 */
int mp_write_vusb_with_response(struct mp_handle_t *d, int len, char *src, int dlen, char *dst) {
    int cnt;

    cnt = usb_control_msg(d->phandle,USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
                          VENDOR_RQ_WRITE_BUFFER, 0, 0, src, len, 5000);

    if(cnt < 0) {
        fprintf(stderr, "Error on write buffer: %s\n", usb_strerror());
        return FALSE;
    }

    if(dlen) {
        cnt = usb_control_msg(d->phandle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                              VENDOR_RQ_READ_BUFFER, 0, 0, dst, dlen, 5000);
        if(cnt < 0) {
            fprintf(stderr, "Error on read buffer: %s\n", usb_strerror());
            return FALSE;
        }

        if(cnt != dlen) {
            fprintf(stderr, "Short read.  Expecting %d, got %d\n", dlen, cnt);
            return FALSE;
        }
    }

    return TRUE;
}



/**
 * write to the device in a device specific manner,
 * returning a buffer of the size expected.
 *
 * @returns TRUE on success, with dst filled to
 * dlen, FALSE otherwise
 */
int mp_write_usb_with_response(struct mp_handle_t *d, int len, char *src, int dlen,
                               char *dst) {

    switch(d->comm_protocol) {
    case COMM_PROTOCOL_PIC:
        if(mp_write_usb(d, len, src)) {
            return mp_recv_usb(d, dlen, dst);
        } else {
            return FALSE;
        }
        break;
    case COMM_PROTOCOL_VUSB:
        return mp_write_vusb_with_response(d, len, src, dlen, dst);
    default:
        break;
    }

    return FALSE;
}



/*
 * read from i2c device
 */
int mp_i2c_read(struct mp_handle_t *d, unsigned char dev, unsigned char addr, unsigned char len, unsigned char *data) {
    char *buf;
    int result = FALSE;
    int retval;

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

    debug_printf("executing mp_i2c_read: dev 0x%02x, addr 0x%02x, len 0x%02x", dev, addr, len);

    if((result = mp_write_usb_with_response(d, 5, buf, len + 1, buf))) {
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
int mp_i2c_write(struct mp_handle_t *d, unsigned char dev, unsigned char addr, unsigned char len, unsigned char *data) {
    char *buf;
    int result = FALSE;
    int retval;

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

    debug_printf("executing mp_i2c_write: dev 0x%02x, addr 0x%02x, len 0x%02x", dev, addr, len);

    if((result = mp_write_usb_with_response(d, len + 4, buf, 2, buf))) {
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
int mp_read_eeprom(struct mp_handle_t *d, unsigned char addr, unsigned char *retval) {
    char buf[3];
    int result;

    if(!d->has_eeprom)
        return FALSE;

    buf[0] = CMD_READ_EEPROM;
    buf[1] = 1;
    buf[2] = addr;

    debug_printf("executing mp_read_eeprom: %d", addr);

    if((result = mp_write_usb_with_response(d, 3, buf, 2, buf))) {
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

    debug_printf("executing mp_write_eeprom: addr %d -> %d", addr, value);

    if(mp_write_usb_with_response(d, 4, buf, 4, buf)) {
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

    debug_printf("executing mp_power_set: %d",state);

    return mp_write_usb_with_response(d, 3, buf, 1, buf);
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
    char buf[8];
    int index;
    int result;
    struct mp_i2c_handle_t *pi2c;

    // we'll assume we already have the space allocated, and
    // phandle is already assigned

    // First, get version info
    debug_printf("Getting version info");
    if(!mp_write_usb_with_response(d, 2, "\0\0", 2, buf))
        return FALSE;

    d->fw_major = (int) buf[0];
    d->fw_minor = (int) buf[1];

    // Get board type info
    debug_printf("Getting board info");
    if(!mp_write_usb_with_response(d, 2, "\x30\1", 4, buf))
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
    debug_printf("Getting board specific info");
    switch(d->board_id) {
    case BOARD_TYPE_POWER:
        if(!mp_write_usb_with_response(d, 2, "\x31\2", 2, buf))
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
                        pi2c->i2c_type = i2c_type[I2C_UNKNOWN];
                        if(pi2c->i2c_id < I2C_UNKNOWN)
                            pi2c->i2c_type = i2c_type[pi2c->i2c_id];
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
 * create a new handle, given a usb device
 */
struct mp_handle_t *mp_create_handle(struct usb_device *device) {
    struct mp_handle_t *pnew;
    usb_dev_handle *phandle;


    debug_printf("Checking device 0x%04x:0x%04x", device->descriptor.idVendor,
                 device->descriptor.idProduct);

    if(((device->descriptor.idVendor != mp_vendorID) ||
       (device->descriptor.idProduct != mp_productID)) &&
       ((device->descriptor.idVendor != mp_vusb_vendorID) ||
        (device->descriptor.idProduct != mp_vusb_productID))) {
        debug_printf("Not valid");
        return NULL;
    }

    phandle = usb_open(device);
    if(!phandle) {
        debug_printf("Error in usb_open");
        return NULL;
    }

    if(usb_set_configuration(phandle, mp_configuration) < 0) {
        debug_printf("Error in set_configuration");
        usb_close(phandle);
        return NULL;
    }

    if (usb_claim_interface(phandle, mp_interface) < 0) {
        debug_printf("Error in claim_interface");
        usb_close(phandle);
        return NULL;
    }

    pnew = (struct mp_handle_t *)malloc(sizeof(struct mp_handle_t));
    if(!pnew) {
        usb_release_interface(phandle, mp_interface);
        usb_close(phandle);
        return NULL;
    }

    if((device->descriptor.idVendor == mp_vendorID) &&
       (device->descriptor.idProduct == mp_productID))
        pnew->comm_protocol = COMM_PROTOCOL_PIC;
    else
        pnew->comm_protocol = COMM_PROTOCOL_VUSB;

    pnew->phandle = phandle;
    if(!mp_query_info(pnew)) {
        debug_printf("Couldn't query info");
        mp_destroy_handle(pnew);
        return NULL;
    }

    return pnew;
}


/*
 * destroy a previously allocated mp handle
 */
void mp_destroy_handle(struct mp_handle_t *ph) {
    usb_reset(ph->phandle);
    usb_release_interface(ph->phandle, mp_interface);
    usb_close(ph->phandle);

    /* FIXME: Walk the i2c devices */
    free(ph);
}

/*
 * release a handle
 */
void mp_release_handle(struct mp_handle_t *ph) {
    usb_reset(ph->phandle);
    usb_release_interface(ph->phandle, mp_interface);
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
            printf("\nSerial   Firmware     Proc   Mhz EEPROM  Type\n");
            printf("----------------------------------------------"
                   "-----------------\n");
            found = 1;
        }
        printf("%04d       %d.%02d  %-8s   %-3d   %s  %s\n",
               pmp->serial,
               pmp->fw_major,
               pmp->fw_minor,
               pmp->processor_type,
               pmp->processor_speed,
               pmp->has_eeprom ? "Yes" : " No",
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
                       pi2c->device, pi2c->mpusb ? pi2c->i2c_type :
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

/* Find the first USB device with this vendor and product.
 *  Exits on errors, like if the device couldn't be found. -osl
 *
 * This function is heavily based upon Orion Sky Lawlor's
 *  usb_pickit program, which was a very useful reference
 *  for all the USB stuff.  Thanks!
 */
struct mp_handle_t *mp_open(int type, int id) {
    struct mp_handle_t *pmp;

    pmp = devicelist.pnext;
    while(pmp) {
        if(((pmp->board_id == type) || (type == BOARD_TYPE_ANY)) &&
           ((id == BOARD_SERIAL_ANY) || (id == pmp->serial))) {

            if(usb_set_configuration(pmp->phandle, mp_configuration) < 0) {
                debug_printf("Error in set_configuration");
                return NULL;
            }

            if (usb_claim_interface(pmp->phandle, mp_interface) < 0) {
                debug_printf("Error in claim_interface");
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
    struct usb_device *device;
    struct usb_bus* bus;
    struct mp_handle_t *pmp;

    devicelist.pnext = NULL;

#ifdef MUST_BE_ROOT
    if (geteuid()!=0) {
        fprintf(stderr,"This program must be run as root.\n");
        exit(1);
    }
#endif

#ifdef USB_DEBUG
    usb_debug=4;
#endif

    //    printf("Locating Monkey Puppet Labs USB device (vendor 0x%04x/product 0x%04x)\n",
    //           mp_vendorID,mp_productID);

    /* (libusb setup code stolen from John Fremlin's cool "usb-robot") -osl */
    usb_init();
    usb_find_busses();
    usb_find_devices();

    for (bus=usb_get_busses();bus!=NULL;bus=bus->next) {
        struct usb_device* usb_devices = bus->devices;
        for(device=usb_devices;device!=NULL;device=device->next) {
            pmp = mp_create_handle(device);
            if(pmp) {
                pmp->pnext = devicelist.pnext;
                devicelist.pnext = pmp;
                mp_release_handle(pmp);
            }
        }
    }
    return TRUE;
}

/**
 * release everything
 */
void mp_deinit(void) {
    struct mp_handle_t *pmp;

    pmp = devicelist.pnext;

    while(pmp) {
        devicelist.pnext = pmp->pnext;
        mp_destroy_handle(pmp);
        pmp = devicelist.pnext;
    }

    // no usb_deinit();
}
