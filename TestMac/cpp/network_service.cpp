#include "network_service.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <memory.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/kern_control.h>
#include <sys/uio.h>
#include <sys/sys_domain.h>
#include <netinet/ip.h>
#include <net/if_utun.h> //here defined _NET_IF_UTUN_H_

#include "log.h"
#include "fileutl.h"
#include "sockutl.h"
#include "ssl_client2.h"
#include "stringutl.h"
#include "http_client.h"
#include "sockhttp.h"
#include "sysutl.h"
#include "timeutl.h"
#include "obfuscation_utl.h"

const int BUF_SIZE = 4096*4;
int g_protocol = kSslType;
uint32_t g_private_ip;
std::string global_private_ip;
std::string global_default_gateway_ip;
int g_fd_tun_dev;
std::string target_test_ip; //="159.65.226.184";
uint16_t target_test_port;
std::string web_server_ip = "www.tinyvpn.xyz";
static uint32_t in_traffic, out_traffic;
static int g_isRun;
static int client_sock;
static int g_in_recv_tun;
static int g_in_recv_socket;

int premium = 2;
std::string g_user_name;// = "dudu@163.com";
std::string g_password;// = "123456";
std::string g_device_id;
static int firstOpen=0;
//int current_traffic = 0 ;  //bytes
uint32_t g_day_traffic;
uint32_t g_month_traffic;

int popen_fill( char* data, int len, const char* cmd, ... )
{
    FILE* output;
    va_list vlist;
    char line[2048];
    int line_len, line_count, total_len, id;

    if (!data || len <= 0 || !cmd)
        return -1;

    va_start(vlist, cmd);
    line_len = vsnprintf(line, 2048, cmd, vlist);
    if (line_len < 0) line_len = 0;
    va_end(vlist);
    line[line_len] = 0;

    line_count = id = total_len = 0;
    if ((output = popen(line, "r"))) {
        while (!feof(output) && fgets(line, sizeof(line)-1, output)){
            line_len = (int)strlen(line);
            if (len - total_len - 1 < line_len){
                break;
            }
            memcpy(data + total_len, line, line_len);
            total_len += line_len;
            line_count++;
            id++;
        }
        pclose(output);
    }
    data[total_len] = 0;
    return total_len;
}
int set_dns()
{
    char ret_data[256];
    INFO("networksetup -listnetworkserviceorder | grep '(1)' | awk '{print $2}'");
    if (popen_fill(ret_data, sizeof(ret_data), "networksetup -listnetworkserviceorder | grep '(1)' | awk '{print $2}'") <= 0) {
        ERROR("networksetup cmd listnetworkserviceorder fail.");
        return 1;
    }
    for (int i=0;i<strlen(ret_data);i++)
        if (ret_data[i] == 0x0A)
            ret_data[i] = ' ';
    
    std::string str_cmd;
    str_cmd = std::string("networksetup -getdnsservers ") + ret_data;
    std::string str_network_card = ret_data;
    INFO("cmd: %s", str_cmd.c_str());
    if (popen_fill(ret_data, sizeof(ret_data), str_cmd.c_str()) > 0) {
        for (int i=0;i<strlen(ret_data);i++)
            if (ret_data[i] == 0x0A)
                ret_data[i] = ' ';
        str_cmd = ret_data;

        //INFO("networksetup getdnsservers ok: %s", string_utl::HexEncode(std::string(ret_data, strlen(ret_data))).c_str());
        INFO("networksetup getdnsservers ok: %s", ret_data);
        if (str_cmd.find("8.8.8.8") != std::string::npos)
            return 0;
        if (str_cmd.find("any") != std::string::npos) {
            str_cmd = std::string("networksetup -setdnsservers ") + str_network_card + " 8.8.8.8";
            INFO("cmd: %s", str_cmd.c_str());
            system(str_cmd.c_str());
            return 0;
        }
        str_cmd = std::string("networksetup -setdnsservers ") + str_network_card + std::string("8.8.8.8 ") + str_cmd;
        INFO("cmd: %s", str_cmd.c_str());
        system(str_cmd.c_str());
        return 0;
    }
    str_cmd = std::string("networksetup -setdnsservers ") + str_network_card + " 8.8.8.8";
    INFO("cmd: %s", str_cmd.c_str());
    system(str_cmd.c_str());
    return 0;
}
bool ifconfig(const char * ifname, const char * va, const char *pa)
{
    char cmd[2048] = {0};
    snprintf(cmd, sizeof(cmd), "ifconfig %s %s %s netmask 255.255.0.0 up",ifname, va, pa);
    INFO("ifconfig:%s\n", cmd);
    if (system(cmd) < 0)
    {
        ERROR("sys_utl::ifconfig,interface(%s) with param(%s) and params(%s) fail at error:%d\n",ifname,va,pa,errno);
        return false;
    }
    return true;
}

void exception_signal_handler(int nSignal)
{
    INFO("capture exception signal: %d",nSignal);
    
    if(global_default_gateway_ip.size() > 0)
    {
        system("sudo route delete default");
        
        std::string add_org_gateway_cmd("sudo route add default ");
        add_org_gateway_cmd += global_default_gateway_ip;
        system(add_org_gateway_cmd.c_str());
    }
}
void quit_signal_handler(int nSignal)
{
    INFO("capture quit signal: %d",nSignal);
    if(global_default_gateway_ip.size() > 0)
    {
        system("sudo route delete default");
        
        std::string add_org_gateway_cmd("sudo route add default ");
        add_org_gateway_cmd += global_default_gateway_ip;
        system(add_org_gateway_cmd.c_str());
    }
    exit(0);
}
void add_host_to_route(const std::string ip, const std::string device)
{
    std::string routing_cmd("route add -host ");
    routing_cmd += ip.c_str();
    routing_cmd += (" -interface ");
    routing_cmd += device;
    system(routing_cmd.c_str());
    INFO("routing_cmd:%s", routing_cmd.c_str());
}

void add_host_to_gateway(const std::string ip, const std::string gateway)
{
    std::string routing_cmd("route add -host ");
    routing_cmd += ip.c_str();
    routing_cmd += (" -gateway ");
    routing_cmd += gateway;
    system(routing_cmd.c_str());
    INFO("routing_cmd:%s", routing_cmd.c_str());
}
void delete_host_to_gateway(const std::string ip, const std::string gateway)
{
    std::string routing_cmd("route delete -host ");
    routing_cmd += ip.c_str();
    routing_cmd += (" -gateway ");
    routing_cmd += gateway;
    system(routing_cmd.c_str());
    INFO("routing_cmd:%s", routing_cmd.c_str());
}

void add_network_to_route(const std::string network, const std::string device)
{
    std::string routing_cmd("route add -net ");
    routing_cmd += network.c_str();
    routing_cmd += (" -interface ");
    routing_cmd += device;
    system(routing_cmd.c_str());
}

void add_network_to_gateway(const std::string network,std::string net_mask,const std::string gateway)
{
    std::string routing_cmd("route add -net ");
    routing_cmd += network.c_str();
    routing_cmd += " netmask ";
    routing_cmd += net_mask.c_str();
    routing_cmd += (" -gateway ");
    routing_cmd += gateway;
    system(routing_cmd.c_str());
}
static char g_tcp_buf[BUF_SIZE*2];
static int g_tcp_len;
int write_tun(char* ip_packet_data, int ip_packet_len){
    int len;
    if (g_tcp_len != 0) {
        if (ip_packet_len + g_tcp_len > sizeof(g_tcp_buf)) {
            ERROR("relay size over %lu", sizeof(g_tcp_buf));
            g_tcp_len = 0;
            return 1;
        }
        memcpy(g_tcp_buf + g_tcp_len, ip_packet_data, ip_packet_len);
        ip_packet_data = g_tcp_buf;
        ip_packet_len += g_tcp_len;
        g_tcp_len = 0;
        DEBUG2("relayed packet:%d", ip_packet_len);
    }

    while(1) {
        if (ip_packet_len == 0)
            break;
        // todo: recv from socket, send to utun1
        if (ip_packet_len < sizeof(struct ip) ) {
            ERROR("less than ip header:%d.", ip_packet_len);
            memcpy(g_tcp_buf, ip_packet_data, ip_packet_len);
            g_tcp_len = ip_packet_len;
            break;
        }
        struct ip *iph = (struct ip *)ip_packet_data;
        len = ntohs(iph->ip_len);

        if (ip_packet_len < len) {
            if (len > BUF_SIZE) {
                ERROR("something error1.%x,%x,data:%s",len, ip_packet_len, string_utl::HexEncode(std::string(ip_packet_data,ip_packet_len)).c_str());
                g_tcp_len = 0;
            } else {
                DEBUG2("relay to next packet:%d,current buff len:%d", ip_packet_len, g_tcp_len);
                if (g_tcp_len == 0) {
                    memcpy(g_tcp_buf +g_tcp_len, ip_packet_data, ip_packet_len);
                    g_tcp_len += ip_packet_len;
                }
            }
            break;
        }

        if (len > BUF_SIZE) {
            ERROR("something error.%x,%x",len, ip_packet_len);
            g_tcp_len = 0;
            break;
        } else if (len == 0) {
            ERROR("len is zero.%x,%x",len, ip_packet_len); //string_utl::HexEncode(std::string(ip_packet_data,ip_packet_len)).c_str());
            g_tcp_len = 0;
            break;
        }

        char ip_src[INET_ADDRSTRLEN + 1];
        char ip_dst[INET_ADDRSTRLEN + 1];
        inet_ntop(AF_INET,&iph->ip_src.s_addr,ip_src, INET_ADDRSTRLEN);
        inet_ntop(AF_INET,&iph->ip_dst.s_addr,ip_dst, INET_ADDRSTRLEN);

        DEBUG2("send to utun, from(%s) to (%s) with size:%d",ip_src,ip_dst,len);
        if (sys_utl::tun_dev_write(g_fd_tun_dev, (void*)ip_packet_data, len) <= 0) {
        //if (::write(g_fd_tun_dev, (void*)ip_packet_data, len) <= 0) {
            ERROR("write tun error:%d", g_fd_tun_dev);
            return 1;
        }
        ip_packet_len -= len;
        ip_packet_data += len;
    }
    return 0;
}
int write_tun_http(char* ip_packet_data, int ip_packet_len) {
    static uint32_t g_iv = 0x87654321;
    int len;
    if (g_tcp_len != 0) {
        if (ip_packet_len + g_tcp_len > sizeof(g_tcp_buf)) {
            INFO("relay size over %d", sizeof(g_tcp_buf));
            g_tcp_len = 0;
            return 1;
        }
        memcpy(g_tcp_buf + g_tcp_len, ip_packet_data, ip_packet_len);
        ip_packet_data = g_tcp_buf;
        ip_packet_len += g_tcp_len;
        g_tcp_len = 0;
        INFO("relayed packet:%d", ip_packet_len);
    }
    std::string http_packet;
    int http_head_length, http_body_length;
    while (1) {
        if (ip_packet_len == 0)
            break;
        http_packet.assign(ip_packet_data, ip_packet_len);
        if (sock_http::pop_front_xdpi_head(http_packet, http_head_length, http_body_length) != 0) {  // decode http header fail
            DEBUG2("relay to next packet:%d,current buff len:%d", ip_packet_len, g_tcp_len);
            if (g_tcp_len == 0) {
                memcpy(g_tcp_buf + g_tcp_len, ip_packet_data, ip_packet_len);
                g_tcp_len += ip_packet_len;
            }
            break;
        }
        ip_packet_len -= http_head_length;
        ip_packet_data += http_head_length;
        obfuscation_utl::decode((unsigned char *) ip_packet_data, 4, g_iv);
        obfuscation_utl::decode((unsigned char *) ip_packet_data + 4, http_body_length - 4, g_iv);

        struct ip *iph = (struct ip *) ip_packet_data;
        len = ntohs(iph->ip_len);
        char ip_src[INET_ADDRSTRLEN + 1];
        char ip_dst[INET_ADDRSTRLEN + 1];
        inet_ntop(AF_INET, &iph->ip_src.s_addr, ip_src, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &iph->ip_dst.s_addr, ip_dst, INET_ADDRSTRLEN);

        DEBUG2("send to tun,http, from(%s) to (%s) with size:%d, header:%d,body:%d", ip_src,
              ip_dst, len, http_head_length, http_body_length);
        sys_utl::tun_dev_write(g_fd_tun_dev, (void *) ip_packet_data, len);

        ip_packet_len -= http_body_length;
        ip_packet_data += http_body_length;
    }
    return 0;
}
int network_service::tun_and_socket(void(*trafficCallback)(long, long,long,long,void*), void* target)
{
    INFO("start ssl thread,g_fd_tun_dev:%d,client_sock:%d", g_fd_tun_dev, client_sock);
    g_isRun= 1;
    g_in_recv_tun = 1;
    g_tcp_len = 0;
    in_traffic = 0;
    out_traffic = 0;
    //std::thread tun_thread(client_recv_tun, client_sock);

    g_in_recv_socket = 1;
    int ip_packet_len;
    char ip_packet_data[BUF_SIZE];
    int ret;
    time_t lastTime = time_utl::localtime();
    time_t currentTime;
    time_t recvTime = time_utl::localtime();
    time_t sendTime = time_utl::localtime();

    fd_set fdsr;
    int maxfd;
    while(g_isRun == 1){
        FD_ZERO(&fdsr);
        FD_SET(client_sock, &fdsr);
        FD_SET(g_fd_tun_dev, &fdsr);
        maxfd = std::max(client_sock, g_fd_tun_dev);
        struct timeval tv_select;
        tv_select.tv_sec = 2;
        tv_select.tv_usec = 0;
        int nReady = select(maxfd + 1, &fdsr, NULL, NULL, &tv_select);
        if (nReady < 0) {
            ERROR("select error:%d", nReady);
            break;
        } else if (nReady == 0) {
            INFO("select timeout");
            continue;
        }

        if (FD_ISSET(g_fd_tun_dev, &fdsr)) {  // recv from tun
            static VpnPacket vpn_packet(4096);
            int readed_from_tun;
            vpn_packet.reset();
            readed_from_tun = sys_utl::tun_dev_read(g_fd_tun_dev, vpn_packet.data(), vpn_packet.remain_size());
            vpn_packet.set_back_offset(vpn_packet.front_offset()+readed_from_tun);
            if(readed_from_tun < sizeof(struct ip)) {
                ERROR("tun_dev_read error, size:%d", readed_from_tun);
                break;
            }
            if(readed_from_tun > 0)
            {
                struct ip *iph = (struct ip *)vpn_packet.data();

                char ip_src[INET_ADDRSTRLEN + 1];
                char ip_dst[INET_ADDRSTRLEN + 1];
                inet_ntop(AF_INET,&iph->ip_src.s_addr,ip_src, INET_ADDRSTRLEN);
                inet_ntop(AF_INET,&iph->ip_dst.s_addr,ip_dst, INET_ADDRSTRLEN);

                if(g_private_ip != iph->ip_src.s_addr) {
                    ERROR("src_ip mismatch:%x,%x",g_private_ip, iph->ip_src.s_addr);
                    continue;
                }
                DEBUG2("recv from tun, from(%s) to (%s) with size:%d",ip_src,ip_dst,readed_from_tun);
                //file_utl::write(sockid, vpn_packet.data(), readed_from_tun);
                out_traffic += readed_from_tun;
                if (g_protocol == kSslType) {
                    if (ssl_write(vpn_packet.data(), readed_from_tun) != 0) {
                        ERROR("ssl_write error");
                        break;
                    }
                } else if (g_protocol == kHttpType){
                    http_write(vpn_packet);
                }
                sendTime = time_utl::localtime();
            }
//            if (--nReady == 0)  // read over
//                continue;
        }
        if (FD_ISSET(client_sock, &fdsr)) {  // recv from socket
            ip_packet_len = 0;
            if (g_protocol == kSslType) {
                ret = ssl_read(ip_packet_data, ip_packet_len);
                if (ret != 0) {
                    ERROR("ssl_read error");
                    break;
                }
            } else if (g_protocol == kHttpType) {
                ip_packet_len = file_utl::read(client_sock, ip_packet_data, BUF_SIZE);
            } else {
                ERROR("protocol error.");
                break;
            }
            if (ip_packet_len == 0)
                continue;
            in_traffic += ip_packet_len;
           DEBUG2("recv from socket, size:%d", ip_packet_len);
            if (g_protocol == kSslType) {
                if (write_tun((char *) ip_packet_data, ip_packet_len) != 0) {
                    ERROR("write_tun error");
                    break;
                }
            } else if (g_protocol == kHttpType){
                if (write_tun_http((char *) ip_packet_data, ip_packet_len) != 0) {
                    ERROR("write_tun error");
                    break;
                }
            }
            recvTime = time_utl::localtime();
        }
        currentTime = time_utl::localtime();
        if (currentTime - recvTime > 60 || currentTime - sendTime > 60) {
            ERROR("send or recv timeout");
            break;
        }
        if (currentTime - lastTime >= 1) {
            /*jclass thisClass = (env)->GetObjectClass(thisObj);
            jfieldID fidNumber = (env)->GetFieldID(thisClass, "current_traffic", "I");
            if (NULL == fidNumber) {
                ERROR("current_traffic error");
                return 1;
            }
            // Change the variable
            (env)->SetIntField(thisObj, fidNumber, in_traffic + out_traffic);
            INFO("current traffic:%d", in_traffic+out_traffic);
            jmethodID mtdCallBack = (env)->GetMethodID(thisClass, "trafficCallback", "()I");
            if (NULL == mtdCallBack) {
                ERROR("trafficCallback error");
                return 1;
            }
            ret = (env)->CallIntMethod(thisObj, mtdCallBack);
            */
            trafficCallback(g_day_traffic + (in_traffic + out_traffic)/1024, g_month_traffic+ (in_traffic + out_traffic)/1024, 0,0, target);
            lastTime = time_utl::localtime();
        }
    }

    INFO("main thread stop");
    if(g_protocol == kSslType)
        ssl_close();
    else if (g_protocol == kHttpType)
        close(client_sock);
    close(g_fd_tun_dev);
    g_isRun = 0;

    // callback to app
/*    jclass thisClass = (env)->GetObjectClass( thisObj);
    jmethodID midCallBackAverage = (env)->GetMethodID(thisClass,   "stopCallback", "()I");
    if (NULL == midCallBackAverage)
        return 1;
    ret = (env)->CallIntMethod(thisObj, midCallBackAverage);
    __android_log_print(ANDROID_LOG_DEBUG, "JNI", "stop callback:%d", ret);*/
    return 0;
}
int network_service::init() {
    signal(SIGKILL, quit_signal_handler);
    signal(SIGINT, quit_signal_handler);
    
    OpenFile("/tmp/tvpn.log");
    SetLogLevel(0);
    firstOpen = 1;
    
    system("route -n get default | grep 'gateway' | awk '{print $2}' > /tmp/default_gateway.txt");
    file_utl::read_file("/tmp/default_gateway.txt", global_default_gateway_ip);
    if(global_default_gateway_ip.size() > 0)
    {
        //remove \n \r
        const int last_char = *global_default_gateway_ip.rbegin();
        if( (last_char == '\n') || (last_char == '\r') )
            global_default_gateway_ip.resize(global_default_gateway_ip.size()-1);
    }
    string_utl::set_random_http_domains();
    sock_http::init_http_head();

    return 0;
}
int network_service::get_vpnserver_ip(std::string user_name, std::string password, std::string device_id, long premium, std::string country_code, void(*trafficCallback)(long, long,long,long,void*), void* target){
    struct hostent *h;
    if((h=gethostbyname(web_server_ip.c_str()))==NULL) {
        return 1;
    }
    std::string web_ip = inet_ntoa(*((struct in_addr *)h->h_addr));
    INFO("web ip:%s", web_ip.c_str());
    struct sockaddr_in serv_addr;
    int sock =socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        INFO("socket() error");
        return 1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=inet_addr(web_ip.c_str());
    serv_addr.sin_port=htons(60315);
    
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) {
        INFO("connect() error!");
        return 1;
    }
    INFO("connect web server ok.");
    
/*    std::string::size_type     found = country_code.find("\"");
    if (found == std::string::npos)
        return 1;
    std::string strCountry = country_code.substr(found+1);
    found = country_code.find("\"");
    if (found == std::string::npos)
        return 1;
    strCountry = strCountry.substr(0,found - 1);
    INFO("country code:%s",strCountry.c_str());*/

    std::string strtemp;
    strtemp += (char)0;
    strtemp += (char)premium;
    strtemp += country_code;
    if (premium <= 1)
        strtemp += device_id;
    else
        strtemp += user_name;
    INFO("send:%s", string_utl::HexEncode(strtemp).c_str());
    int ret=file_utl::write(sock, (char*)strtemp.c_str(), (int)strtemp.size());
    char ip_packet_data[BUF_SIZE];
    ret=file_utl::read(sock, ip_packet_data, BUF_SIZE);

    ip_packet_data[ret] = 0;
    INFO("recv from web_server:%s", string_utl::HexEncode(std::string(ip_packet_data,ret)).c_str());
    //current_traffic = 0;
    int pos = 0;
    uint32_t day_traffic = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t month_traffic = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t day_limit = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t month_limit = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    g_day_traffic = day_traffic;
    g_month_traffic = month_traffic;
    if (premium<=1){
        if (day_traffic > day_limit) {
            trafficCallback(day_traffic,month_traffic,day_limit, month_limit, target);
            return 2;
        }
    } else{
        if (month_traffic > month_limit) {
            trafficCallback(day_traffic,month_traffic,day_limit, month_limit, target);
            return 2;
        }
    }
    trafficCallback(day_traffic,month_traffic,day_limit, month_limit, target);
    
    std::string recv_ip(ip_packet_data + 16, ret-16);
    std::vector<std::string> recv_data;
    string_utl::split_string(recv_ip,',', recv_data);
    if(recv_data.size() < 1) {
        ERROR("recv server ip data error.");
        //tunnel.close();
        return 1;
    }
    INFO("recv:%s", recv_data[0].c_str());
    std::vector<std::string>  server_data;
    string_utl::split_string(recv_data[0],':', server_data);

    if(server_data.size() < 3) {
        ERROR("parse server ip data error.");
        //tunnel.close();
        return 1;
    }
    //Log.i(TAG, "data:" + server_data[0]+","+server_data[1]+","+server_data[2]);
    g_protocol = std::stoi(server_data[0]);
    target_test_ip = server_data[1];
    //g_ip = "192.168.50.218";
    target_test_port = std::stoi(server_data[2]);
    INFO("protocol:%d,%s,%d",g_protocol, target_test_ip.c_str(),target_test_port);
    return 0;
}
int network_service::start_vpn(std::string user_name, std::string password, std::string device_id, long premium, std::string country_code, void(*stopCallback)(long, void*), void(*trafficCallback)(long, long,long,long,void*), void* target)
{
    if(firstOpen==0) {
        init();
    }
    g_user_name = user_name;
    g_password = password;
    g_device_id = device_id;
    // get vpnserver ip
    int ret = get_vpnserver_ip(user_name, password, device_id, premium, country_code, trafficCallback, target);
    if ( ret == 1) {
        stopCallback(1, target);
        return 1;
    } else if (ret == 2) {
        stopCallback(1, target);
        return 2;
    }
    
    if (connect_server()!=0){
        stopCallback(1, target);
        return 1;
    }
    
    std::string dev_name;
    g_fd_tun_dev = sys_utl::open_tun_device(dev_name, false);
    if (g_fd_tun_dev <= 0) {
        INFO("open_tun_device error");
        stopCallback(1, target);
        return 1;
    }
    socket_utl::set_nonblock(g_fd_tun_dev,false);
    ifconfig(dev_name.c_str(),global_private_ip.c_str(),global_private_ip.c_str());

    
    if(global_default_gateway_ip.size() <= 0){
        stopCallback(1, target);
        INFO("global_default_gateway_ip empty.");
        return 1;
    }

    add_host_to_gateway(target_test_ip,global_default_gateway_ip);
    std::string add_default_gateway_cmd("sudo route add default ");
    add_default_gateway_cmd += global_private_ip;
    system(add_default_gateway_cmd.c_str());
    INFO("add_default_gateway_cmd:%s", add_default_gateway_cmd.c_str());
    
    std::string change_default_gateway_cmd("sudo route change default -interface ");
    change_default_gateway_cmd += dev_name;
    system(change_default_gateway_cmd.c_str());
    INFO("change_default_gateway_cmd:%s", change_default_gateway_cmd.c_str());
    
    std::string add_org_gateway_cmd("sudo route add default ");
    add_org_gateway_cmd += global_default_gateway_ip;
    system(add_org_gateway_cmd.c_str());
    INFO("add_org_gateway_cmd:%s", add_org_gateway_cmd.c_str());

    add_host_to_route(global_private_ip, dev_name);
    set_dns();
    stopCallback(0, target);
    
    ret = tun_and_socket(trafficCallback, target);
    if (ret != 0) {
        stopCallback(1, target);
        return ret;
    }
    
    delete_host_to_gateway(target_test_ip,global_default_gateway_ip);
    system("sudo route delete default");
    INFO("cmd: sudo route delete default");
    add_org_gateway_cmd = "sudo route add default ";
    add_org_gateway_cmd += global_default_gateway_ip;
    system(add_org_gateway_cmd.c_str());
    INFO("add_org_gateway_cmd:%s", add_org_gateway_cmd.c_str());
    
    stopCallback(1, target);
    return 0;
}
int network_service::connect_server()
{
    int sock =socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
       INFO("socket() error");
       return 1;
    }

    //std::string strIp = "159.65.226.184";
    //uint16_t port = 14455;
    if (g_protocol == kSslType) {
       if (init_ssl_client() != 0) {
           ERROR( "init ssl fail.");
           return 1;
       }
       INFO("connect ssl");
       connect_ssl(target_test_ip, target_test_port, sock);
       if (sock == 0) {
           ERROR("sock is zero.");
           return 1;
       }
    } else if (g_protocol == kHttpType) {
       if (connect_tcp(target_test_ip, target_test_port, sock) != 0)
           return 1;
    } else {
       ERROR( "protocol errror.");
       return 1;
    }
    client_sock = sock;

    INFO("connect ok.");
    std::string strPrivateIp;
    //INFO("get private_ip");
    if (g_protocol == kSslType){
       //std::string strId = "IOS.00000001";
       //std::string strPassword = "123456";
       
       get_private_ip(premium,g_device_id, g_user_name, g_password, strPrivateIp);
    }

    g_private_ip = *(uint32_t*)strPrivateIp.c_str();

    global_private_ip = socket_utl::socketaddr_to_string(g_private_ip);
    printf("private_ip:%s", global_private_ip.c_str());
    //g_private_ip = ntohl(g_private_ip);

    return 0;
}
int network_service::stop_vpn(long value)
{
    g_isRun = 0;
    return 0;
}
int network_service::connect_web_server(std::string user_name, long premium, uint32_t* private_ip )
{
    return 0;
}
int network_service::login(std::string user_name, std::string password, std::string device_id,
                           void(*trafficCallback)(long, long,long,long,long,long, void*), void* target)
{
    if(firstOpen==0) {
        init();
    }
    
    struct hostent *h;
    if((h=gethostbyname(web_server_ip.c_str()))==NULL) {
        return 1;
    }
    std::string web_ip = inet_ntoa(*((struct in_addr *)h->h_addr));
    INFO("web ip:%s", web_ip.c_str());
    struct sockaddr_in serv_addr;
    int sock =socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        INFO("socket() error");
        return 1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=inet_addr(web_ip.c_str());
    serv_addr.sin_port=htons(60315);
    
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) {
        INFO("connect() error!");
        return 1;
    }
    INFO("connect web server ok.");
    /*
    std::string::size_type 	found = country_code.find("\"");
    if (found == std::string::npos)
        return 1;
    std::string strtemp = country_code.substr(found+1);
    found = country_code.find("\"");
    if (found == std::string::npos)
        return 1;
    strtemp = strtemp.substr(0,found - 1);
    INFO("country code:%s",strtemp.c_str());*/

    std::string strtemp;
    strtemp += (char)1;
    strtemp += device_id;
    strtemp += (char)'\n';
    strtemp += user_name;
    strtemp += (char)'\n';
    strtemp += password;

    int ret=file_utl::write(sock, (char*)strtemp.c_str(), (int)strtemp.size());
    char ip_packet_data[BUF_SIZE];
    ret=file_utl::read(sock, ip_packet_data, BUF_SIZE);
    if (ret < 2 + 4*sizeof(uint32_t))
        return 1;
    ip_packet_data[ret] = 0;
    INFO("recv from web_server:%s", string_utl::HexEncode(std::string(ip_packet_data,ret)).c_str());
    int pos=0;
    int ret1 = ip_packet_data[pos++];
    int ret2 = ip_packet_data[pos++];
    uint32_t day_traffic = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t month_traffic = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t day_limit = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t month_limit = ntohl(*(uint32_t*)(ip_packet_data + pos));
    
    close(sock);
    INFO("recv login:%d,%d,%x,%x,%x,%x" , ret1 , ret2, day_traffic , month_traffic, day_limit,month_limit);
/*
    std::string ip_list;
    ip_list.assign(ip_packet_data, ret);
    std :: vector < std :: string > values;
    values.clear();
    string_utl::split_string(ip_list, ',', values);
    if (values.empty()) {
        printf("get ip fail.\n");
        return 1;
    }
    std::string one_ip = values[0];
    values.clear();
    string_utl::split_string(ip_list, ':', values);
    if (values.size() < 3) {
        printf("parse ip addr fail.\n");
        return 1;
    }
    g_protocol = std::stoi(values[0]);
    target_test_ip = values[1];  // need to fix.
    target_test_port = std::stoi(values[2]);
    INFO("protocol:%d, server_ip:%s,port:%d", g_protocol, target_test_ip.c_str(), target_test_port);
    */
    g_user_name = user_name;
    g_password = password;
    
    trafficCallback(day_traffic,month_traffic,day_limit,month_limit,ret1,ret2,target);
    

    return 0;
}
