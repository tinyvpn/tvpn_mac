//
//  network_service.h
//  TinyVpn
//
//  Created by Hench on 7/7/20.
//  Copyright © 2020 Hench. All rights reserved.
//

#ifndef network_service_h
#define network_service_h
#ifdef __cplusplus
#include <string>
#include <iostream>

class network_service {
public:
    int connect_web_server(std::string user_name, long premium, uint32_t* private_ip);
    int start_vpn(std::string user_name, std::string password, std::string device_id, long premium, std::string country_code, void(*stopCallback)(long, void*),void(*trafficCallback)(long, long,long,long,void*), void* target);
    int stop_vpn(long value);
    int login(std::string user_name, std::string password, std::string device_id, void(*trafficCallback)(long, long,long,long,long,long, void*), void* target);
    
    int connect_server();
    int tun_and_socket(void(*trafficCallback)(long, long,long,long,void*), void* target);
    int get_vpnserver_ip(std::string user_name, std::string password, std::string device_id, long premium, std::string country_code, void(*trafficCallback)(long, long,long,long,void*), void* target);
    int init();
};
#endif
#endif /* network_service_h */
