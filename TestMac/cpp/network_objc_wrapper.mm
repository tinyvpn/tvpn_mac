//
//  network_objc_wrapper.m
//  TinyVpn
//
//  Created by Hench on 7/7/20.
//  Copyright © 2020 Hench. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "network_objc_wrapper.h"
#include "network_service.h"
@interface NetworkService() {
    network_service* service;
}
@end
@implementation NetworkService
- (instancetype)init {
    if (self = [super init]) {
        service = new network_service();
    }
    return self;
}

- (void)dealloc {
    delete service;
}
- (NSInteger) connect_web_server: (NSString *)user_name with:(NSInteger) premium ip:(NSInteger*) private_ip{
    service->connect_web_server([user_name UTF8String],premium, (uint32_t*)private_ip);
    return 0;
}
- (NSInteger) start_vpn:(NSString *)userName pwd: (NSString *)password device_id: (NSString *)device_id premium: (NSInteger)premium country_code: (NSString *)country_code stop_call: (void(*)(NSInteger status, void* target)) stopCallback traffic_call: (void(*)(NSInteger todayTraffic, NSInteger monthTraffic, NSInteger dayLimit,NSInteger monthLimit, void* target)) trafficCallback withTarget: (void*) target{
    service->start_vpn([userName UTF8String], [password UTF8String], [device_id UTF8String], premium, [country_code UTF8String], stopCallback, trafficCallback, target);
    return 0;
}
- (NSInteger) stop_vpn:(NSInteger)value {
    service->stop_vpn(value);
    return 0;
}
- (NSInteger) login: (NSString *)userName pwd: (NSString *)password device_id: (NSString *)device_id traffic_call:(void(*)(NSInteger todayTraffic, NSInteger monthTraffic,NSInteger dayLimit,NSInteger monthLimit,NSInteger ret1,NSInteger ret2, void* target)) trafficCallback withTarget: (void*) target {
    service->login([userName UTF8String], [password UTF8String], [device_id UTF8String], trafficCallback, target);
    return 0;
}
@end
