//
//  MPI2CDevice.m
//  mpusb-framework
//
//  Created by Ron Pedde on 5/26/09.
//  Copyright 2009 Rackspace Managed Hosting. All rights reserved.
//

#import "mpusb/mpusb.h"

#import "MPI2CDevice.h"

@implementation MPI2CDevice

-(MPI2CDevice *) initFromDevice: (struct mp_i2c_handle_t *)aDevice {
    self = [ super init ];
    
    _i2cdev = aDevice;
    

    if(_i2cdev->mpusb) {
        boardType = [[ NSString alloc ] initWithCString:_i2cdev->i2c_type ];
    } else {
        boardType = [[ NSString alloc ] initWithFormat: @"Non MPUSB Device" ];
    }
    
    serialString = [[ NSString alloc ] initWithFormat: @"%03d", _i2cdev->device ];
    serial = _i2cdev->device;
    
    return self;
}

-(void) dealloc {
    [ boardType release ];
    [ serialString release ];
    [ super dealloc ];
}


-(NSString *) boardType {
    return boardType;
}

-(NSString *) friendlyName {
    return boardType;
}

-(NSString *) serialString {
    return serialString;
}

@end
