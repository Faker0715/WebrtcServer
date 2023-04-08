//
// Created by faker on 23-4-4.
//

#ifndef XRTCSERVER_NETWORK_H
#define XRTCSERVER_NETWORK_H

#include <string>
#include <rtc_base/ip_address.h>
#include <vector>

namespace xrtc{
    class Network{
    public:
        Network(const std::string& name, const rtc::IPAddress& ip):
                _name(name),_ip(ip){}
                ~Network() = default;
        const std::string& name(){ return _name;}
        const rtc::IPAddress& ip(){ return _ip;}
        std::string to_string(){
            return _name + ":" + _ip.ToString();
        }
    private:
        std::string _name;
        rtc::IPAddress _ip;
    };

    class NetworkManager{
    public:
        NetworkManager();
        ~NetworkManager();
        const std::vector<Network*>& get_networks(){
            return _network_list;
        }
        int create_networks();
    private:
        std::vector<Network*> _network_list;
    };
}


#endif //XRTCSERVER_NETWORK_H
