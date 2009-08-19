//
//  mpusbapi.h
//  mpusb-framework
//
//  Created by Ron Pedde on 5/8/09.
//  Copyright 2009 Ron Pedde. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "mpusbdevice.h"

@interface MPUSBAPI : NSObject {
    NSMutableArray *controllerArray;
}

+ (MPUSBAPI *) getInstance;

- (void) dealloc;

- (MPUSBDevice*) getControllerByType:(int)aType;
- (MPUSBDevice*) getControllerBySerial:(int)aSerial;
- (NSMutableArray*) getControllerSerials;
- (NSMutableArray*) getAllControllers;

@end

