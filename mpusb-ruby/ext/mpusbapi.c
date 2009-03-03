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
static VALUE cMPUSBI2CDevice;

/* dumb logging functions */
static void eprintf(char *fmt, ...);
static void xprintf(char *fmt, ...);

typedef struct mpusb_deviceinfo_t {
    struct mp_handle_t *dev;
    int opened;
} DEVICEINFO;

typedef struct mpusb_i2cdeviceinfo_t {
    struct mp_i2c_handle_t *dev;
} I2CDEVICEINFO;

/* Forwards */
static VALUE mpusb_init(VALUE self);
static VALUE mpusb_open(VALUE self, VALUE type, VALUE serial);
static VALUE mpusb_devicelist(VALUE self);
static VALUE mpusb_getdebug(VALUE self);
static VALUE mpusb_setdebug(VALUE self, VALUE newvalue);

static VALUE mpusb_i2c_default_max(VALUE self, VALUE newvalue);
static VALUE mpusb_i2c_default_min(VALUE self, VALUE newvalue);

static VALUE mpdevice_init(int argc, VALUE *argv, VALUE self);
static VALUE mpdevice_allocate(VALUE klass);
static void mpdevice_free(DEVICEINFO *self);

static VALUE mpi2c_init(int argc, VALUE *argv, VALUE self);
static VALUE mpi2c_allocate(VALUE klass);
static void mpi2c_free(I2CDEVICEINFO *self);

static int mpusb_debug = 0;

/*
 * optionally print debug info
 */
void eprintf(char *fmt, ...) {
    va_list ap;

    if(!mpusb_debug)
        return;

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

    eprintf("Initialized mpusb\n");
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

    pdev->opened = 1;

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

static VALUE mpusb_getdebug(VALUE self) {
    return mpusb_debug ? Qtrue : Qfalse;
}

static VALUE mpusb_setdebug(VALUE self, VALUE newvalue) {
    mpusb_debug = (newvalue == Qfalse) ? 0 : 1;
    mp_set_debug(NUM2INT(newvalue));
    return newvalue == Qfalse ? Qfalse : Qtrue;
}

static VALUE mpusb_i2c_default_max(VALUE self, VALUE newvalue) {
    mp_i2c_default_max(NUM2INT(newvalue));
    return newvalue;
}

static VALUE mpusb_i2c_default_min(VALUE self, VALUE newvalue) {
    mp_i2c_default_min(NUM2INT(newvalue));
    return newvalue;
}

static VALUE mpdevice_init(int argc, VALUE *argv, VALUE self) {
    /* the initializer is either a serial, or a wrapped mp_handle_t */
    DEVICEINFO *pdev;
    I2CDEVICEINFO *pi2cdev;
    VALUE dw;
    struct mp_i2c_handle_t *pi2c;

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

    pdev->opened = 1;

    tmpv = rb_str_new2(pdev->dev->board_type);
    rb_iv_set(self,"@board_type",tmpv);

    tmpv = rb_str_new2(pdev->dev->processor_type);
    rb_iv_set(self,"@processor_type", tmpv);

    tmpv = rb_int_new(pdev->dev->processor_speed);
    rb_iv_set(self,"@processor_speed", tmpv);

    tmpv = rb_int_new(pdev->dev->board_id);
    rb_iv_set(self,"@board_id", tmpv);

    tmpv = rb_int_new(pdev->dev->has_eeprom);
    rb_iv_set(self,"@has_eeprom", tmpv);

    sprintf(firmware_version,"%d.%02d",pdev->dev->fw_major,
            pdev->dev->fw_minor);

    tmpv = rb_str_new2(firmware_version);
    rb_iv_set(self,"@firmware_version", tmpv);

    tmpv = rb_int_new(pdev->dev->serial);
    rb_iv_set(self,"@serial", tmpv);

    if(pdev->dev->board_id == BOARD_TYPE_POWER) {
        tmpv = rb_int_new(pdev->dev->power.devices);
        rb_iv_set(self,"@power_devices", tmpv);
        tmpv = rb_int_new(pdev->dev->power.current);
        rb_iv_set(self,"@power_current", tmpv);
    } else {
        rb_iv_set(self,"@power_devices", Qnil);
        rb_iv_set(self,"@power_current", Qnil);
    }

    if(pdev->dev->board_id == BOARD_TYPE_I2C) {
        tmpv = rb_ary_new();
        pi2c = pdev->dev->i2c_list.pnext;
        while(pi2c) {
            eprintf("Adding new i2c device\n");
            dw = Data_Make_Struct(cMPUSBI2CDevice, I2CDEVICEINFO, 0, mpi2c_free, pi2cdev);
            pi2cdev->dev = pi2c;
            rb_obj_call_init(dw, 0, NULL);
            rb_ary_push(tmpv, dw);
            pi2c = pi2c->pnext;
        }
        rb_iv_set(self, "@i2c_devices", tmpv);
    } else {
        rb_iv_set(self, "@i2c_devices", Qnil);
    }

    return self;
}

static VALUE mpdevice_power_set(VALUE self, VALUE device_id, VALUE state) {
    DEVICEINFO *pdev;
    int i_device, i_state;
    int i_result;

    Data_Get_Struct(self, DEVICEINFO, pdev);

    i_device = NUM2INT(device_id);
    if(state == Qfalse) {
        i_state = 0;
    } else {
        i_state = 1;
    }

    // The device id is currently ignored.  Needs to be fixed
    // for firmware versions that support multiple power devices
    i_result = mp_power_set(pdev->dev, i_state);
    return INT2FIX((int) i_result);
}

static VALUE mpdevice_read_eeprom(VALUE self, VALUE address) {
    DEVICEINFO *pdev;
    int result;
    unsigned char retval;

    Data_Get_Struct(self, DEVICEINFO, pdev);
    result = mp_read_eeprom(pdev->dev, NUM2INT(address), &retval);
    if(result)
        return INT2FIX((int) retval);

    // need to raise an error
    rb_raise(rb_eException,"Cannot read eeprom register\n");
    return Qnil;
}

static VALUE mpdevice_write_eeprom(VALUE self, VALUE address, VALUE value) {
    DEVICEINFO *pdev;
    int result;
    unsigned char retval;

    Data_Get_Struct(self, DEVICEINFO, pdev);
    result = mp_write_eeprom(pdev->dev, NUM2INT(address), NUM2INT(value));
    if(result)
        return Qtrue;

    /* need to raise an error */
    rb_raise(rb_eException,"Cannot write eeprom register\n");
    return Qnil;
}

static VALUE mpdevice_i2c_read(VALUE self, VALUE device_id, VALUE address, VALUE len) {
    DEVICEINFO *pdev;
    int result;
    unsigned char retval;
    unsigned char *buffer;
    VALUE return_buffer;

    Data_Get_Struct(self, DEVICEINFO, pdev);
    buffer = (unsigned char *)malloc(len);

    if(!buffer) {
        rb_raise(rb_eException, "Out of memory");
        return Qfalse;
    }

    result = mp_i2c_read(pdev->dev,
                         (unsigned char) NUM2INT(device_id),
                         (unsigned char) NUM2INT(address),
                         (unsigned char) NUM2INT(len),
                         buffer);

    if(!result) {
        free(buffer);
        rb_raise(rb_eException, "Cannot read from device");
        return Qnil;
    }

    return_buffer = rb_str_new2((char*)buffer);
    free(buffer);
    return return_buffer;
}

static VALUE mpdevice_i2c_write(VALUE self, VALUE device_id, VALUE address, VALUE data) {
    DEVICEINFO *pdev;
    int result;
    unsigned char retval;
    int len;

    len = strlen((char *)RSTRING(data)->ptr);

    Data_Get_Struct(self, DEVICEINFO, pdev);
    result = mp_i2c_write(pdev->dev,
                          (unsigned char)NUM2INT(device_id),
                          (unsigned char) NUM2INT(address),
                          (unsigned char) len,
                          (unsigned char *)RSTRING(data)->ptr);

    if(result)
        return Qtrue;

    /* need to raise an error */
    rb_raise(rb_eException,"Cannot write i2c device");
    return Qfalse;
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

static VALUE mpi2c_allocate(VALUE klass) {
    I2CDEVICEINFO *pdev;

    pdev = malloc(sizeof(I2CDEVICEINFO));
    memset(pdev, 0, sizeof(I2CDEVICEINFO));

    eprintf("Allocated i2c device struct\n");
    return Data_Wrap_Struct(klass, 0, mpi2c_free, pdev);
}

static void mpi2c_free(I2CDEVICEINFO *self) {
    eprintf("Freeing I2C device\n");
    free(self);
}

static VALUE mpi2c_init(int argc, VALUE *argv, VALUE self) {
    /* the initializer is either a i2c address, or a wrapped mp_i2c_handle */
    I2CDEVICEINFO *pdev;
    VALUE tmpv;
    VALUE v_id;

    rb_scan_args(argc, argv, "01", &v_id);
    if(!NIL_P(v_id)) {
        /* should probe for that device */
        eprintf("Making i2c device for addr %d\n",NUM2INT(v_id));
        return Qnil;
    } else {
        eprintf("Making i2c device from existing device info\n");
        Data_Get_Struct(self, I2CDEVICEINFO, pdev);
    }

    if(!pdev->dev) {
        eprintf("Oops... can't find device info\n");
        return Qnil;
    }

    tmpv = rb_int_new(pdev->dev->device);
    rb_iv_set(self,"@i2c_addr", tmpv);

    tmpv = rb_int_new(pdev->dev->mpusb);
    rb_iv_set(self,"@is_mpusb", tmpv);

    tmpv = rb_int_new(pdev->dev->i2c_id);
    rb_iv_set(self,"@i2c_id", tmpv);

    tmpv = rb_str_new2(pdev->dev->i2c_type);
    rb_iv_set(self,"@i2c_type", tmpv);

    return self;
}


void Init_mpusbapi() {
    cMPUSB = rb_define_class("MPUSBAPI",rb_cObject);

    rb_define_const(cMPUSB, "BOARD_TYPE_ANY", INT2FIX(BOARD_TYPE_ANY));
    rb_define_const(cMPUSB, "BOARD_TYPE_POWER", INT2FIX(BOARD_TYPE_POWER));
    rb_define_const(cMPUSB, "BOARD_TYPE_I2C", INT2FIX(BOARD_TYPE_I2C));
    rb_define_const(cMPUSB, "BOARD_TYPE_NEOGEO", INT2FIX(BOARD_TYPE_NEOGEO));
    rb_define_const(cMPUSB, "BOARD_TYPE_UNKNOWN", INT2FIX(BOARD_TYPE_UNKNOWN));

    rb_define_const(cMPUSB, "BOARD_SERIAL_ANY", INT2FIX(BOARD_SERIAL_ANY));

    rb_define_const(cMPUSB, "PROCESSOR_TYPE_2450", INT2FIX(PROCESSOR_TYPE_2450));
    rb_define_const(cMPUSB, "PROCESSOR_TYPE_2550", INT2FIX(PROCESSOR_TYPE_2550));

    rb_define_const(cMPUSB, "I2C_HD44780", INT2FIX(I2C_HD44780));
    rb_define_const(cMPUSB, "I2C_UNKNOWN", INT2FIX(I2C_UNKNOWN));

    rb_define_const(cMPUSB, "I2C_DEFAULT_MIN", INT2FIX(I2C_LOW));
    rb_define_const(cMPUSB, "I2C_DEFAULT_MAX", INT2FIX(I2C_HIGH));

    rb_define_method(cMPUSB, "initialize", mpusb_init, 0);
    rb_define_method(cMPUSB, "open", mpusb_open, 2);
    rb_define_method(cMPUSB, "devicelist", mpusb_devicelist, 0);

    rb_define_singleton_method(cMPUSB, "i2c_probe_max", mpusb_i2c_default_max, 1);
    rb_define_singleton_method(cMPUSB, "i2c_probe_min", mpusb_i2c_default_min, 1);

    rb_define_singleton_method(cMPUSB, "debug", mpusb_getdebug, 0);
    rb_define_singleton_method(cMPUSB, "debug=", mpusb_setdebug, 1);

    cMPUSBDevice = rb_define_class("MPUSBAPIDevice", rb_cObject);
    rb_define_alloc_func(cMPUSBDevice,mpdevice_allocate);

    rb_define_attr(cMPUSBDevice,"board_type", 1, 0);
    rb_define_attr(cMPUSBDevice,"board_id", 1, 0);
    rb_define_attr(cMPUSBDevice,"processor_type", 1, 0);
    rb_define_attr(cMPUSBDevice,"processor_speed", 1, 0);
    rb_define_attr(cMPUSBDevice,"has_eeprom", 1, 0);
    rb_define_attr(cMPUSBDevice,"firmware_version", 1, 0);
    rb_define_attr(cMPUSBDevice,"serial", 1, 0);
    rb_define_attr(cMPUSBDevice,"power_devices", 1, 0);
    rb_define_attr(cMPUSBDevice,"power_current", 1, 0);
    rb_define_attr(cMPUSBDevice,"i2c_devices", 1, 0);

    rb_define_method(cMPUSBDevice, "initialize", mpdevice_init, -1);
    rb_define_method(cMPUSBDevice, "power_set", mpdevice_power_set, 2);
    rb_define_method(cMPUSBDevice, "read_eeprom", mpdevice_read_eeprom, 1);
    rb_define_method(cMPUSBDevice, "write_eeprom", mpdevice_write_eeprom, 2);
    rb_define_method(cMPUSBDevice, "i2c_read", mpdevice_i2c_read, 3);
    rb_define_method(cMPUSBDevice, "i2c_write", mpdevice_i2c_write, 3);

    cMPUSBI2CDevice = rb_define_class("MPUSBAPII2CDevice", rb_cObject);
    rb_define_alloc_func(cMPUSBI2CDevice, mpi2c_allocate);

    rb_define_attr(cMPUSBI2CDevice, "i2c_addr", 1, 0);
    rb_define_attr(cMPUSBI2CDevice, "is_mpusb", 1, 0);
    rb_define_attr(cMPUSBI2CDevice, "i2c_id", 1, 0);
    rb_define_attr(cMPUSBI2CDevice, "i2c_type", 1, 0);

    rb_define_method(cMPUSBI2CDevice, "initialize", mpi2c_init, -1);
}
