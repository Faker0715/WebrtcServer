#include "base/network.h"

#include <ifaddrs.h>

#include <rtc_base/logging.h>

namespace xrtc {

NetworkManager::NetworkManager() = default;

NetworkManager::~NetworkManager() {
    for (auto network : network_list_) {
        delete network;
    }

    network_list_.clear();
}

int NetworkManager::CreateNetworks() {
    struct ifaddrs* interface;
    int err = getifaddrs(&interface);
    if (err != 0) {
        RTC_LOG(LS_WARNING) << "getifaddrs error: " << strerror(errno) 
            << ", errno: " << errno;
        return -1;
    }
    
    for (auto cur = interface; cur != nullptr; cur = cur->ifa_next) {
        if (cur->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        struct sockaddr_in* addr = (struct sockaddr_in*)(cur->ifa_addr);
        rtc::IPAddress ip_address(addr->sin_addr);

        if(rtc::IPIsLoopback(ip_address)){
            continue;
        }
        
        Network* network = new Network(cur->ifa_name, ip_address);

        RTC_LOG(LS_INFO) << "gathered network interface: " << network->ToString();

        network_list_.push_back(network);
    }
    
    freeifaddrs(interface);

    return 0;
}

} // namespace xrtc


