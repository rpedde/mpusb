//
//  mpusbapi.m
//  mpusb-framework
//
//  Created by Ron Pedde on 5/8/09.
//  Copyright 2009 Ron Pedde. All rights reserved.
//

#import "mpusb/mpusb.h"

#import "mpusbapi.h"
#import "mpusbdevice.h"


@implementation MPUSBAPI
+ (MPUSBAPI *)getInstance {
    static MPUSBAPI *sharedSingleton = NULL;
	
    @synchronized(self) {
        if(!sharedSingleton) {
            sharedSingleton = [[MPUSBAPI alloc] init];
        }
    }
    return sharedSingleton;
}

- (id) init {
    int retval; 

    self = [ super init ];
    
    retval = mp_init();
    controllerArray = [[ NSMutableArray alloc ] init ];
    if(retval == TRUE) {
        /* walk the list of controllers and build device objects
         * from them, populating the controllerArray
         */
        struct mp_handle_t *devlist;
        struct mp_handle_t *current;
        
        devlist = mp_devicelist();
        current = devlist;
        
        while(current) {
            [controllerArray addObject:[[MPUSBDevice alloc ] initFromDevice: current ]];
            current = current->pnext;
        }
    }
    
    return self;
}

- (void) dealloc {
    [ controllerArray release ];
    mp_deinit();
    [ super dealloc ];
}

- (MPUSBDevice*)getControllerByType:(int)aType {
    MPUSBDevice *mpDevice = [MPUSBDevice alloc];
    struct mp_handle_t *usbDevice;
	
    usbDevice = mp_open(aType,BOARD_SERIAL_ANY);
	
    [mpDevice initFromDevice:usbDevice];

    [mpDevice autorelease];
    return mpDevice;
}

- (MPUSBDevice*)getControllerBySerial:(int)aSerial {
    MPUSBDevice *mpDevice = [MPUSBDevice alloc];
    struct mp_handle_t *usbDevice;
	
    usbDevice = mp_open(BOARD_TYPE_ANY,aSerial);
	
    [mpDevice initFromDevice:usbDevice];
	
    [mpDevice autorelease];
    return mpDevice;
}

- (NSMutableArray*)getControllerSerials {
    struct mp_handle_t *devlist;
    struct mp_handle_t *current;
    NSMutableArray *devArray = [[NSMutableArray alloc] init ];
	
    devlist = mp_devicelist();
    current = devlist;
	
    while(current) {
        [devArray addObject:[NSNumber numberWithInt:current->serial]];
        current = current->pnext;
    }
	
    [devArray autorelease];
    return devArray;
}

- (NSMutableArray*)getAllControllers {
    return controllerArray;
}

@end


