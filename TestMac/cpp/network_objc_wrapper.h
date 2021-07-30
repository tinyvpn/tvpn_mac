//
//  network_objc_wrapper.h
//  TinyVpn
//
//  Created by Hench on 7/7/20.
//  Copyright © 2020 Hench. All rights reserved.
//

#ifndef network_objc_wrapper_h
#define network_objc_wrapper_h
#import <Foundation/Foundation.h>

@interface NetworkService : NSObject
//- (void) simpleCall: (void(*)())function;
//- (void) checkIsPrime: (NSString *)value with: (void(*)(void*)) progressCallback andWith: (void(*)(bool result, void* target)) resultCallback withTarget: (void*) target ;
//- (void) mycheck: (NSInteger)value;
- (NSInteger) connect_web_server: (NSString *)value with: (NSInteger)premium ip: (NSInteger*)private_ip;
- (NSInteger) start_vpn: (NSString *)userName pwd: (NSString *)password device_id: (NSString *)device_id premium: (NSInteger)premium country_code: (NSString *)country_code stop_call: (void(*)(NSInteger status, void* target)) stopCallback traffic_call: (void(*)(NSInteger todayTraffic, NSInteger monthTraffic, NSInteger dayLimit,NSInteger monthLimit, void* target)) trafficCallback withTarget: (void*) target;
- (NSInteger) stop_vpn: (NSInteger)value;
- (NSInteger) login: (NSString *)userName pwd: (NSString *)password device_id: (NSString *)device_id traffic_call:(void(*)(NSInteger todayTraffic, NSInteger monthTraffic, NSInteger dayLimit,NSInteger monthLimit,NSInteger ret1,NSInteger ret2,void* target)) trafficCallback withTarget: (void*) target;
@end


#endif /* network_objc_wrapper_h */
