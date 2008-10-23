/*
 * This file is part of fsusb_picdem
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


#include <usb.h> /* libusb header */
#include <unistd.h> /* for geteuid */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "main.h"
#include "mpusb.h"

/* Handler forwards */
int handler_power(struct mp_handle_t *d, int action, int argc, char **argv);
int handler_list(struct mp_handle_t *d, int action, int argc, char **argv);
int handler_eeprom(struct mp_handle_t *d, int action, int argc, char **argv);
int handler_i2c(struct mp_handle_t *d, int action, int argc, char **argv);
int handler_help(struct mp_handle_t *d, int action, int argc, char **argv);

/* Usage forwards */
void usage_power(void);
void usage_list(void);
void usage_eeprom(void);
void usage_i2c(void);
void usage_help(void);

/* Other forwards */
void show_usage(void);
int get_action(char *action);

typedef struct action_t {
    char *action;
    int required_type;
    int requires_device;
    int (*handler)(struct mp_handle_t *,int, int,  char**);
    void(*usage)(void);
} ACTION;


/* Globals */
int done = 0;     // Interactive processing
struct mp_handle_t *mp_current = NULL;
int interactive = 0;


#define ACTION_LIST         0
#define ACTION_POWER        1
#define ACTION_EEPROM       2
#define ACTION_I2C          3
#define ACTION_HELP         4

ACTION action_list[] = {
    { "list",        BOARD_TYPE_ANY,   0, handler_list,   usage_list },
    { "power",       BOARD_TYPE_POWER, 1, handler_power,  usage_power },
    { "eeprom",      BOARD_TYPE_ANY,   1, handler_eeprom, usage_eeprom },
    { "i2c",         BOARD_TYPE_I2C,   1, handler_i2c,    usage_i2c },
    { "help",        BOARD_TYPE_ANY,   0, handler_help,   usage_help },
    { NULL, 0 }
};

#define I2C_E_SUCCESS 0
#define I2C_E_NODpEV   1
#define I2C_E_NOACK   2
#define I2C_E_TIMEOUT 3
#define I2C_E_OTHER   4
#define I2C_E_LAST    5

char *i2c_errors[] = {
    "Success",
    "Invalid device",
    "Protocol error.  Missing ACK",
    "Timeout",
    "Unknown"
};

void *xmalloc(int size) {
    void *result;
    result = malloc(size);
    if(!result) {
        perror("malloc");
        exit(0);
    }

    return result;
}

char *xstrdup(char *str) {
    char *result;

    result = strdup(str);
    if(!result) {
        perror("strdup");
        exit(0);
    }

    return result;
}

void usage_power(void) {
    printf("power <on|off> [options]\n");
    printf(" Power on or off devices attached to a power board\n\n");
    printf("options:\n");
    printf(" -d <device>    device to power on or off.  Defaults to 1\n\n");
}

void usage_list(void){
    printf("list\n");
    printf(" List all connected mpusb devices\n\n");
}

void usage_eeprom(void) {
    printf("eeprom read <addr>\n");
    printf(" Read eeprom value.  Only valid for devices with onboard eeprom (18f2550)\n\n");
    printf("eeprom write <addr> <value> [<value>...]\n");
    printf(" Write eeprom at address <addr> with <value>\n\n");
}

void usage_i2c(void) {
    printf("i2c read <device> <addr> <len>\n");
    printf(" read <len> bytes from address <addr> of i2c device <device>\n\n");
    printf("Note: <addr> is the pre-shifted address\n\n");
}

void usage_help(void) {
    printf("help\n");
    printf(" View usage help (like this!) for a command\n\n");
}

int get_action(char *action) {
    ACTION *pcurrent;
    int counter = 0;

    pcurrent = &action_list[0];
    while(pcurrent->action) {
        if(strcasecmp(pcurrent->action, action) == 0)
            return counter;
        counter = counter + 1;
        pcurrent++;
    }

    return -1;
}

int handler_help(struct mp_handle_t *d, int action, int argc, char **argv) {
    int help_action;
    ACTION *paction;

    if(argc != 1) {
        printf("Valid help targets:\n");
        paction = &action_list[0];
        while(paction->action) {
            printf("  %s\n",paction->action);
            paction++;
        }
        return TRUE;
    }

    help_action = get_action(argv[0]);
    if(help_action == -1) {
        printf("Invalid action: %s\n",argv[0]);
        show_usage();
    }

    action_list[help_action].usage();
    return TRUE;
}

int handler_eeprom(struct mp_handle_t *d, int action, int argc, char **argv) {
    unsigned char result;

    if(!argc) {
        action_list[action].usage();
        exit(1);
    }

    if(strcasecmp(argv[0],"read") == 0) {
        if(mp_read_eeprom(d, atoi(argv[1]), &result)) {
            printf("EEProm value at 0x%02x: 0x%02x\n",atoi(argv[1]),result);
            return TRUE;
        }
        return FALSE;
    } else if (strcasecmp(argv[0],"write") == 0) {
        return mp_write_eeprom(d, atoi(argv[1]), atoi(argv[2]));
    } else {
        action_list[action].usage();
    }

    return FALSE;
}

int handler_i2c(struct mp_handle_t *d, int action, int argc, char **argv) {
    unsigned char buffer[256];
    unsigned char len, device, addr;
    int result;
    int index;

    if(argc < 4) {
        action_list[action].usage();
        exit(1);
    }

    device = atoi(argv[1]);
    addr = atoi(argv[2]);

    if(strcasecmp(argv[0],"read") == 0) {
        len = atoi(argv[3]);

        if((result=mp_i2c_read(d, device, addr, len, &buffer[0]))) {
            printf("Read byte(s): ");
            for (index = 0; index < len; index++) {
                printf("0x%02x ", buffer[index]);
            }

            printf("\n");
            return TRUE;
        } else {
            if(buffer[0] < I2C_E_LAST) {
                printf("Error 0x%02x: %s\n",buffer[0],i2c_errors[buffer[0]]);
            } else {
                printf("I2C Error 0x%02x\n",buffer[0]);
                return FALSE;
            }
        }
    } else {
        action_list[action].usage();
    }

    return FALSE;
}

int handler_power(struct mp_handle_t *d, int action, int argc, char **argv) {
    int result;
    int state = 0;

    if(!argc) {
        action_list[action].usage();
        exit(1);
    }

    if(strcasecmp(argv[0],"on") == 0) {
        state = 1;
    }

    result = mp_power_set(d, state);
    return result;
}

int handler_list(struct mp_handle_t *d, int action, int argc, char **argv) {
    mp_list();
    printf("\n");
    return TRUE;
}

void show_usage(void) {
    ACTION *paction;

    printf("mpusb: Software for Monkey Puppet Labs USB controller\n");
    printf("usage: mpusb [-s <serial>] <action> ... \n\n");
    printf("actions:\n");

    paction = &action_list[0];
    while(paction->action) {
        printf("  %s\n",paction->action);
        paction++;
    }

    printf("\nFor help on specific actions, use 'mpusb help <action>'\n\n");
    exit(1);
}

void initialize_readline(void) {
    // here, we would eventually add hooks to completion functions
}

void execute_line(char *line) {
    char *linecopy, *pcurrent;
    int argc = 0;
    int current = 0;
    char **argv;
    int action;

    linecopy = xstrdup(line);

    while(strsep(&linecopy," \t") != NULL)
        argc++;

    argv = (char**)(xmalloc(argc * sizeof(char*)));

    free(linecopy);
    linecopy = xstrdup(line);

    while((pcurrent = strsep(&linecopy," \t")) != NULL) {
        argv[current] = pcurrent;
        current++;
    }

    // check for special actions
    if(strcasecmp(argv[0],"quit") == 0) {
        done = 1;
        free(linecopy);
        return;
    } else if(strcasecmp(argv[0],"exit") == 0) {
        done = 1;
        free(linecopy);
        return;
    } else if(strcasecmp(argv[0],"attach") == 0) {
        if(argc != 2) {
            printf("attach <serial>\n Attach to device with serial <serial>\n");
            free(linecopy);
            return;
        }

        if(mp_current)
            mp_destroy_handle(mp_current);

        mp_current = mp_open(BOARD_TYPE_ANY,atoi(argv[1]));
        if(!mp_current) {
            printf("Could not find board with serial %d.\n",atoi(argv[1]));
            free(linecopy);
            return;
        }

        free(linecopy);
        return;
    }

    // got the argv and argc, now find out what the hell we are doing...
    action = get_action(argv[0]);
    if(action == -1) {
        printf("Invalid action: %s\n",argv[0]);
        printf("Valid actions: \n");
        printf(" attach <serial>\n");
        printf(" exit\n");

        action = 0;

        while(action_list[action].action) {
            printf(" %s\n",action_list[action].action);
            action++;
        }
        free(linecopy);
        return;
    }

    if(!action_list[action].requires_device) {
        action_list[action].handler(NULL, action, argc-1,&argv[1]);
    } else {
        if(!mp_current) {
            printf("No device currently attached.  Use 'attach <serial>' first\n");
        } else {
            if((action_list[action].required_type != BOARD_TYPE_ANY) &&
               (mp_current->board_type != action_list[action].required_type)) {
                printf("Current board type (%s) does not support this action\n",
                       board_type[mp_current->board_type]);
            } else {
                // can do!
                action_list[action].handler(mp_current,action,argc-1,&argv[1]);
            }
        }
    }
    free(linecopy);
}


int do_interactive(void) {
    char *line;
    char prompt[40];

    initialize_readline();
    while(!done) {
        if(!mp_current) {
            strcpy(prompt,"\x1b[32m(none)\x1b[0m> ");
        } else {
            snprintf(prompt,sizeof(prompt),"\x1b[32m%s\x1b[0m> ",
                     board_type[mp_current->board_type]);
        }

        line = readline(prompt);
        if(line) {
            add_history(line);
            printf("\x1b[37m");
            execute_line(line);
        }
        free(line);
    }

    return 1;
}

int main(int argc, char *argv[]) {
    int option;
    int id = BOARD_SERIAL_ANY;
    char **callback_argv = NULL;
    int callback_argc = 0;
    int action;
    int index;
    int retval;
    struct mp_handle_t *usbdev;

    printf("Monkey Puppet Labs USB interface.  Version %d.%02d\n", VERSION_MAJOR, VERSION_MINOR);
    printf("Copyright (c) 2008 Monkey Puppet Labs.  All rights reserved.\n\n");

    while((option = getopt(argc, argv, "+s:hi")) != -1) {
        switch(option) {
        case 's':
            id =  atoi(optarg);
            break;
        case 'h':
            show_usage();
            break;
        case 'i':
            interactive = 1;
            break;
        default:
            show_usage();
            break;
        }
    }

    if(interactive) {
        do_interactive();
        exit(0);
    }

    if(optind == argc)
        show_usage();

    if(argc - (optind + 1)) {
        callback_argc = argc - (optind + 1);
        //        printf("Processing %d args.\n",callback_argc);
        callback_argv = (char**)malloc(sizeof(char *) * callback_argc);
        if(!callback_argv) {
            perror("malloc");
            exit(1);
        }

        for (index = optind + 1; index < argc; index++)
            callback_argv[index - (optind + 1)] = argv[index];
    }

    action = get_action(argv[optind]);
    if(action == -1) {
        printf("Invalid action: %s\n",argv[optind]);
        show_usage();
        exit(1);
    }

    if(!action_list[action].requires_device) {
        action_list[action].handler(NULL,action, callback_argc, callback_argv);
    } else {
        usbdev = mp_open(action_list[action].required_type, id);
        if(!usbdev) {
            printf("Could not find board of type %d (%s) with serial %04d %s\n",
                   action_list[action].required_type,
                   board_type[action_list[action].required_type],
                   id, id == BOARD_SERIAL_ANY ? "(ANY)" : "");
            exit(-1);
        }

        retval = action_list[action].handler(usbdev, action,
                                             callback_argc, callback_argv);
        printf("Result: %s\n", retval ? "success" : "failure");
    }

    return 0;
}
