//
//  mpusbdevice.m
//  mpusb-framework
//
//  Created by Ron Pedde on 5/9/09.
//  Copyright 2009 Ron Pedde. All rights reserved.
//

#import "mpusb/mpusb.h"

#import "MPUSBDevice.h"
#import "MPI2CDevice.h"

@implementation MPUSBDevice

-(MPUSBDevice *)initFromDevice:(struct mp_handle_t *)aDevice {
    struct mp_i2c_handle_t *pi2c;

    self = [super init];
	
    _usbdev = aDevice;
    
    boardType = [[ NSString alloc ] initWithCString:_usbdev->board_type ];
    boardNumber = (enum boardKind) _usbdev->board_id;

    processorType = [[ NSString alloc ] initWithCString:_usbdev->processor_type ];
    processorNumber = (enum processorKind) _usbdev->processor_id;

    processorSpeed = _usbdev->processor_speed;
    serial = _usbdev->serial;
    serialString = [[ NSString alloc ] initWithFormat: @"%03d", serial ];
    
    firmwareVersion = (double) _usbdev->fw_major + ((double) _usbdev->fw_minor / 100.0);
    
    friendlyName = [[ NSString alloc ] initWithFormat: @"%@ (%1.02f)", boardType, firmwareVersion ];
    
    /* fill in subdevices (i2c devices) */
    subDevices = [[ NSMutableArray alloc ] init ];
    
    if(_usbdev->board_id == BOARD_TYPE_I2C) {
        NSLog(@"Adding I2C Device");
        pi2c = _usbdev->i2c_list.pnext;
        while(pi2c) {
            MPI2CDevice *newDevice = (MPI2CDevice *)[[ MPI2CDevice alloc ] initFromDevice: pi2c ];
            [ subDevices addObject: newDevice ];
            [ newDevice release ];
            pi2c = pi2c->pnext;
        }
        NSLog(@"Have %d devices", [subDevices count ]);
    }
    
    return self;
}

- (void) dealloc {
    [ boardType release ];
    [ processorType release ];
    [ friendlyName release ];
    [ subDevices release ];
    [ super dealloc ];
}

-(NSString *) boardType {
    return boardType;
}
						   
-(NSString *) processorType {
    return processorType;
}

-(NSString *) friendlyName {
    return friendlyName;
}

-(NSString *) serialString {
    return serialString;
}

-(NSMutableArray *) subDevices {
    return subDevices;
}

-(double) firmwareVersion {
    return firmwareVersion;
}

-(int)processorSpeed {
    return processorSpeed;
}

-(int)serial {
    return serial;
}

@end

