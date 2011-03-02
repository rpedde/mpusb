/*
** This file is based on fsusb_picdem
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

struct mp_i2c_handle_t {
    int device;
    int mpusb;
    int i2c_id;
    char *i2c_type;
    struct mp_i2c_handle_t *pnext;
};

struct mp_handle_t {
    struct libusb_device_handle *phandle;

    int comm_protocol;
    int board_id;
    char *board_type;

    int processor_id;
    char *processor_type;

    int processor_speed;
    int has_eeprom;
    int serial;
    int fw_major;
    int fw_minor;
    int i2c_devices;
    struct {
        int devices;
        int current;
    } power;
    struct {
        int devices;
    } genio;
    struct mp_i2c_handle_t i2c_list;
    struct mp_handle_t *pnext;
};


#define COMM_PROTOCOL_PIC    0x00
#define COMM_PROTOCOL_VUSB   0x01

#define BOARD_TYPE_ANY       0x00
#define BOARD_TYPE_POWER     0x01
#define BOARD_TYPE_I2C       0x02
#define BOARD_TYPE_UNKNOWN   0x03

#define BOARD_SERIAL_ANY     0x00

#define PROCESSOR_TYPE_2450    0x00
#define PROCESSOR_TYPE_2550    0x01
#define PROCESSOR_TYPE_A168    0x02
#define PROCESSOR_TYPE_A88     0x03
#define PROCESSOR_TYPE_UNKNOWN 0x04

#define I2C_16F690_BOOTLOADER  0x00
#define I2C_HD44780            0x01
#define I2C_SERVO              0x02
#define I2C_IO                 0x03
#define I2C_UNKNOWN            0x04

#define I2C_LOW            0x08
#define I2C_HIGH           0x77


#ifndef TRUE
# define TRUE 1
#endif

#ifndef FALSE
# define FALSE 0
#endif

/* get rid of these... */
extern char *board_type[];
extern char *processor_type[];

/* External Functions */
extern int mp_init(void);
extern void mp_deinit(void);

extern struct mp_handle_t *mp_open(int type, int id);
extern void mp_close(struct mp_handle_t *d);
extern int mp_list(void);
extern struct mp_handle_t *mp_devicelist(void);
extern void mp_set_debug(int value);

/* Power functions */
extern int mp_power_set(struct mp_handle_t *d, int state);

/* EEProm functions */
extern int mp_read_eeprom(struct mp_handle_t *d, unsigned char addr, unsigned char *retval);
extern int mp_write_eeprom(struct mp_handle_t *d, unsigned char addr, unsigned char value);

/* i2c functions */
extern int mp_i2c_read(struct mp_handle_t *d, unsigned char dev, unsigned char addr, unsigned char len, unsigned char *data);
extern int mp_i2c_write(struct mp_handle_t *d, unsigned char dev, unsigned char addr, unsigned char len, unsigned char *data);
extern int mp_i2c_default_min(int min);
extern int mp_i2c_default_max(int max);

/* request and response objects */

#define CMD_READ_VERSION   0x00
#define CMD_READ_EEDATA    0x01
#define CMD_WRITE_EEDATA   0x02
#define CMD_BOARD_TYPE     0x30
#define CMD_BD_POWER_INFO  0x31
#define CMD_BD_POWER_STATE 0x32
#define CMD_I2C_READ       0x40
#define CMD_I2C_WRITE      0x41
#define CMD_RESET          0xFF

/* return value from I2C commands */
#define I2C_E_SUCCESS      0x00  /* Success */
#define I2C_E_NODEV        0x01  /* Invalid device */
#define I2C_E_NOACK        0x02  /* Protocol error.  Missing ACK */
#define I2C_E_TIMEOUT      0x03  /* Timeout */
#define I2C_E_STRETCH      0x04  /* Clock stretch error */
#define I2C_E_OTHER        0x05  /* Unknown */
#define I2C_E_LAST         0x06


#define PACKED __attribute__((aligned(8)))

/* these will get cleaned up later */

/* CMD_READ_VERSION */
typedef struct usb_cmd_read_version_t {
    uint8_t cmd; /* CMD_READ_VERSION */
    uint8_t len; /* length of remaining buffer (0) */
} usb_cmd_read_version_t PACKED;

typedef struct usb_response_read_version_t {
    uint8_t version_major;
    uint8_t version_minor;
} usb_response_read_version_t PACKED;

/* CMD_READ_EEDATA */
typedef struct usb_cmd_read_eedata_t {
    uint8_t cmd; /* CMD_READ_EEDATA */
    uint8_t len; /* length of remaining buffer (1) */
    uint8_t addr;
} usb_cmd_read_eedata_t PACKED;

typedef struct usb_response_read_eedata_t {
    uint8_t value;
    uint8_t addr; /* seriously, this is inexplicable */
} usb_response_read_eedata_t PACKED;

/* CMD_WRITE_EEDATA */
typedef struct usb_cmd_write_eedata_t {
    uint8_t cmd; /* CMD_WRITE_EEDATA */
    uint8_t len; /* length of remaining buffer (2) */
    uint8_t addr;
    uint8_t value;
} usb_cmd_write_eedata_t PACKED;

typedef struct usb_response_write_eedata_t {
    uint8_t result; /* 1 on success, 0 on failure */
    uint8_t len;
    uint8_t addr;
    uint8_t value;  /* you can't make this stuff up */
} usb_response_write_eedata_t PACKED;

/* CMD_BOARD_TYPE */
typedef struct usb_cmd_board_type_t {
    uint8_t cmd; /* CMD_BOARD_TYPE */
    uint8_t len; /* 0 */
} usb_cmd_board_type_t PACKED;

typedef struct usb_response_board_type_t {
    uint8_t board_type; /* BOARD_TYPE_* */
    uint8_t serial;     /* for identification */
    uint8_t proc_type;  /* PROCESSOR_TYPE_* */
    uint8_t mhz;        /* processor MHz */
} usb_response_board_type_t PACKED;

/* CMD_BD_POWER_INFO - BOARD_TYPE_POWER only */
typedef struct usb_cmd_power_info_t {
    uint8_t cmd; /* CMD_BD_POWER_INFO */
    uint8_t len; /* 2 (?!?!?!) */
} usb_cmd_power_info_t PACKED;

typedef struct usb_response_power_info_t {
    uint8_t current; /* amps */
    uint8_t devices; /* controlled devices */
} usb_response_power_info_t PACKED;

/* CMD_BD_POWER_STATE - BOARD_TYPE_POWER only */
typedef struct usb_cmd_power_state_t {
    uint8_t cmd; /* CMD_BD_POWER_STATE */
    uint8_t device; /* which device, 0 based */
    uint8_t state; /* 0 or 1 */
} usb_cmd_power_state_t PACKED;

typedef struct usb_response_power_state_t {
    uint8_t result; /* 1 on success, 0 on failure */
} usb_response_power_state_t PACKED;

/* CMD_I2C_READ - BOARD_TYPE_I2C only */
typedef struct usb_cmd_i2c_read_t {
    uint8_t cmd; /* CMD_I2C_READ */
    uint8_t len; /* 2 (?!?!?!) */
    uint8_t device;
    uint8_t address;
    uint8_t read_len; /* bytes to read */
} usb_cmd_i2c_read_t PACKED;

/* on success... */
typedef struct usb_response_i2c_read_t {
    uint8_t result;
    uint8_t *data;  /* read_len bytes.. */
} usb_response_i2c_read_t PACKED;

/* on failure, result = 0, *data = extended error code */

/* CMD_I2C_WRITE - BOARD_TYPE_I2C only */
typedef struct usb_cmd_i2c_write_t {
    uint8_t cmd; /* CMD_I2C_READ */
    uint8_t len; /* 2 + write_len */
    uint8_t device;
    uint8_t address;
    uint8_t *data; /* write_len derived from len */
} usb_cmd_i2c_write_t PACKED;

typedef struct usb_response_i2c_write_t {
    uint8_t result;
    uint8_t extended_result;
} usb_response_i2c_write_t PACKED;

/* CMD_RESET */
typedef struct usb_cmd_reset_t {
    uint8_t cmd; /* CMD_RESET */
} usb_cmd_reset_t PACKED;

typedef struct usb_response_reset_t {
    uint8_t result;
} usb_response_reset_t PACKED;

#endif /* __MPUSB_H__ */
