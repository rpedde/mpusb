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
#include "main.h"
#include "mpusb.h"

int handler_powerstate(struct mp_handle_t *d, int action, int argc, char **argv);
int handler_list(struct mp_handle_t *d, int action, int argc, char **argv);

typedef struct action_t {
    char *action;
    int required_type;
    int requires_device;
    int (*handler)(struct mp_handle_t *,int, int,  char**);
} ACTION;

#define ACTION_LIST       0
#define ACTION_POWERON    1
#define ACTION_POWEROFF   2

ACTION action_list[] = {
    { "list",       BOARD_TYPE_ANY,   0, handler_list },
    { "poweron",    BOARD_TYPE_POWER, 1, handler_powerstate },
    { "poweroff",   BOARD_TYPE_POWER, 1, handler_powerstate },
    { NULL, 0 }
};


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

int handler_powerstate(struct mp_handle_t *d, int action, int argc, char **argv) {
    int result;

    result = mp_power_set(d, action == ACTION_POWERON ? 1 : 0);
    return result;
}

int handler_list(struct mp_handle_t *d, int action, int argc, char **argv) {
    mp_list();
    printf("\n");
    return TRUE;
}


void show_usage(void) {
    printf("mpusb: Software for Monkey Puppet Labs USB controller\n");
    printf("usage: mpusb [-s <serial>] <action>\n\n");
    printf("actions:\n");
    printf("  list        list all attached mpl boards\n");
    printf("  poweron     power on a power board\n");
    printf("  poweroff    power off a power board\n");
    exit(1);
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

    while((option = getopt(argc, argv, "s:")) != -1) {
        switch(option) {
        case 's':
            id =  atoi(optarg);
            break;
        default:
            show_usage();
            break;
        }
    }

    if(optind == argc)
        show_usage();

    if(argc - (optind + 1)) {
        callback_argc = argc - (optind + 1);
        printf("Processing %d args.\n",callback_argc);
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
            printf("Could not find board of type %s with serial %04d %s\n",
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