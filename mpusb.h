/*
** This file is part of fsusb_picdem
**
** fsusb_picdem is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License as
** published by the Free Software Foundation; either version 2 of the
** License, or (at your option) any later version.
**
** fsusb_picdem is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with fsusb_picdem; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
** 02110-1301, USA
*/


#ifndef __MPUSB_H__
#define __MPUSB_H__

#include <stdint.h>

struct mp_handle_t {
    struct usb_dev_handle *phandle;

    int board_type;
    int processor_type;
    int processor_speed;
    int has_eeprom;
    int serial;
    int fw_major;
    int fw_minor;
    union {
        struct {
            int devices;
            int current;
        } power;
        struct {
            int devices;
        } genio;
    };
};

#define BOARD_TYPE_ANY       0x00
#define BOARD_TYPE_POWER     0x01
#define BOARD_TYPE_I2C       0x02
#define BOARD_TYPE_NEOGEO    0x03

#define BOARD_SERIAL_ANY     0x00

#define PROCESSOR_TYPE_2450  0x00
#define PROCESSOR_TYPE_2550  0x01

extern char *board_type[];
extern char *processor_type[];

/* External Functions */
extern struct mp_handle_t *mp_open(int type, int id);
extern void mp_close(struct mp_handle_t *d);
extern int mp_list(void);

/* Power functions */
extern int mp_power_set(struct mp_handle_t *d, int state);

/* EEProm functions */
extern int mp_read_eeprom(struct mp_handle_t *d, unsigned char addr, unsigned char *retval);
extern int mp_write_eeprom(struct mp_handle_t *d, unsigned char addr, unsigned char value);

/* i2c functions */
extern int mp_i2c_read(struct mp_handle_t *d, unsigned char dev, unsigned char addr, unsigned char len, unsigned char *data);
extern int mp_i2c_write(struct mp_handle_t *d, unsigned char dev, unsigned char addr, unsigned char len, unsigned char *data);

#endif /* __MPUSB_H__ */
