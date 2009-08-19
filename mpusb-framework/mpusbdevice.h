//
//  mpusbdevice.h
//  mpusb-framework
//
//  Created by Ron Pedde on 5/9/09.
//  Copyright 2009 Ron Pedde. All rights reserved.
//

#import <Cocoa/Cocoa.h>

enum processorKind { pk18F2450, pk18F2550, pkUnknown };

enum boardKind { bkAny, bkPower, bkI2C, bkNeoGeo, bkUnknown };

@interface MPUSBDevice : NSObject {
    struct mp_handle_t *_usbdev;
    
    NSString *boardType;
    NSString *processorType;
    NSString *friendlyName;
    NSString *serialString;
    NSMutableArray *subDevices;
    enum processorKind processorNumber;
    enum boardKind boardNumber;
    int processorSpeed;
    int serial;
    double firmwareVersion;
}

-(MPUSBDevice *)initFromDevice:(struct mp_handle_t *)aDevice;
-(void) dealloc;

-(NSString *) boardType;
-(NSString *) processorType;
-(NSString *) friendlyName;
-(NSString *) serialString;
-(NSMutableArray *) subDevices;
-(double) firmwareVersion;
-(int)processorSpeed;
-(int)serial;

@end
