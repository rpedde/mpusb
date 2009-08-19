//
//  MPI2CDevice.h
//  mpusb-framework
//
//  Created by Ron Pedde on 5/26/09.
//  Copyright 2009 Rackspace Managed Hosting. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface MPI2CDevice : NSObject {
    struct mp_i2c_handle_t *_i2cdev;
    NSString *boardType;
    NSString *friendlyName;
    NSString *serialString;
    int serial;
}

-(MPI2CDevice *) initFromDevice:(struct mp_i2c_handle_t *)aDevice;
-(void) dealloc;

-(NSString *) boardType;
-(NSString *) friendlyName;
-(NSString *) serialString;


@end
