/*
 * $Id: $
 *
 * Created by Ron Pedde on 2008-11-03.
 * Copyright (C) 2007 Ron Pedde. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdarg.h>
#include <mpusb/mpusb.h>
#include "ruby.h"

static VALUE cMPUSB;
static VALUE cMPUSBDevice;

/* dumb logging functions */
static void eprintf(char *fmt, ...);
static void xprintf(char *fmt, ...);

typedef struct mpusb_deviceinfo_t {
    struct mp_handle_t *dev;
    int opened;
} DEVICEINFO;


/* Forwards */
static VALUE mpusb_init(VALUE self);
static VALUE mpusb_open(VALUE self, VALUE type, VALUE serial);
static VALUE mpdevice_init(int argc, VALUE *argv, VALUE self);
static void mpdevice_free(DEVICEINFO *self);
static VALUE mpdevice_allocate(VALUE klass);
static VALUE mpusb_devicelist(VALUE self);

/*
 * optionally print debug info
 */
void eprintf(char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
}

void xprintf(char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
}

static VALUE mpusb_init(VALUE self) {
    if(!mp_init()) {
        rb_raise(rb_eException,"Cannot initialize mpusb\n");
        return Qnil;
    }

    eprintf("Initialized mpusb");
    return self;
}

static VALUE mpusb_open(VALUE self, VALUE type, VALUE serial) {
    DEVICEINFO *pdev;
    VALUE dw;
    int i_type, i_serial;

    i_type = NUM2INT(type);
    i_serial = NUM2INT(serial);

    dw = Data_Make_Struct(cMPUSBDevice, DEVICEINFO, 0, mpdevice_free, pdev);
    pdev->dev = mp_open(i_type, i_serial);
    if(!pdev->dev) {
        eprintf("Could not get a device with mp_open\n");
    }

    rb_obj_call_init(dw, 0, NULL);

    return dw;
}

static VALUE mpusb_devicelist(VALUE self) {
    VALUE v_ary_device;
    VALUE device;
    VALUE dw;
    DEVICEINFO *pdev;
    struct mp_handle_t *devlist;
    struct mp_handle_t *pcurrent;

    devlist = mp_devicelist();
    if(!devlist)
        return Qnil;

    v_ary_device = rb_ary_new();

    pcurrent = devlist;
    while(pcurrent) {
        dw = Data_Make_Struct(cMPUSBDevice, DEVICEINFO, 0, mpdevice_free, pdev);
        pdev->dev = pcurrent;
        rb_obj_call_init(dw, 0, NULL);
        rb_ary_push(v_ary_device, dw);
        pcurrent = pcurrent->pnext;
    }

    return v_ary_device;
}


static VALUE mpdevice_init(int argc, VALUE *argv, VALUE self) {
    /* the initializer is either a serial, or a wrapped mp_handle_t */
    DEVICEINFO *pdev;
    VALUE tmpv;
    char firmware_version[20];
    VALUE v_type, v_serial;

    rb_scan_args(argc, argv, "02", &v_type, &v_serial);
    if(NIL_P(v_type))
        v_type = INT2FIX(0);

    if(NIL_P(v_serial))
        v_serial = INT2FIX(1);

    Data_Get_Struct(self, DEVICEINFO, pdev);

    if(!pdev->dev) {
        pdev->dev = mp_open(NUM2INT(v_type), NUM2INT(v_serial));
    }

    if(!pdev->dev)
        return Qnil;

    tmpv = rb_str_new2(pdev->dev->board_type);
    rb_iv_set(self,"@board_type",tmpv);

    tmpv = rb_str_new2(pdev->dev->processor_type);
    rb_iv_set(self,"@processor_type", tmpv);

    tmpv = rb_int_new(pdev->dev->processor_speed);
    rb_iv_set(self,"@processor_speed", tmpv);

    tmpv = rb_int_new(pdev->dev->has_eeprom);
    rb_iv_set(self,"@has_eeprom", tmpv);

    sprintf(firmware_version,"%d.%02d",pdev->dev->fw_major,
            pdev->dev->fw_minor);

    tmpv = rb_str_new2(firmware_version);
    rb_iv_set(self,"@firmware_version", tmpv);

    return self;
}

static void mpdevice_free(DEVICEINFO *self) {
    eprintf("Freeing device\n");

    if(self->opened) {
        eprintf("Releaseing usb device\n");
        mp_close(self->dev);
    }

    free(self);
}

static VALUE mpdevice_allocate(VALUE klass) {
    DEVICEINFO *pdev;

    pdev = malloc(sizeof(DEVICEINFO));
    memset(pdev, 0, sizeof(DEVICEINFO));

    eprintf("Allocated device struct\n");
    // no mark
    return Data_Wrap_Struct(klass, 0, mpdevice_free, pdev);
}


void Init_mpusb() {
    cMPUSB = rb_define_class("MPUSB",rb_cObject);

    rb_define_method(cMPUSB, "initialize", mpusb_init, 0);
    rb_define_method(cMPUSB, "open", mpusb_open, 2);
    rb_define_method(cMPUSB, "devicelist", mpusb_devicelist, 0);

    cMPUSBDevice = rb_define_class("MPUSBDevice", rb_cObject);
    rb_define_alloc_func(cMPUSBDevice,mpdevice_allocate);
    rb_define_attr(cMPUSBDevice,"board_type", 1, 0);
    rb_define_attr(cMPUSBDevice,"processor_type", 1, 0);
    rb_define_attr(cMPUSBDevice,"processor_speed", 1, 0);
    rb_define_attr(cMPUSBDevice,"has_eeprom", 1, 0);
    rb_define_attr(cMPUSBDevice,"firmware_version", 1, 0);

    rb_define_method(cMPUSBDevice, "initialize", mpdevice_init, -1);
}
