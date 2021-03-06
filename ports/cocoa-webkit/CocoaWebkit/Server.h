//
// Copyright © 2017-2018 Atlas Engineer LLC.
// Use of this file is governed by the license that can be found in LICENSE.
//

#import <Foundation/Foundation.h>
#import "GCDWebServer.h"
#import "GCDWebServerDataRequest.h"
#import "GCDWebServerDataResponse.h"
#include "Global.h"
#import "NextApplicationDelegate.h"
#import "XMLRPCRequestDecoder.h"
#import "XMLRPCResponseEncoder.h"

@interface Server : NSObject {
    GCDWebServer* webServer;
    NextApplicationDelegate *delegate;
}

@property (strong, atomic, readwrite) NSMutableDictionary* XMLRPCMethods;

- (void)start;
- (void)stop;
- (void)registerMethod:(NSString*)methodName withBlock:(GCDWebServerDataResponse* (^)(NSArray*))executionBlock;

@end
