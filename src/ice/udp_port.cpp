//
// Created by faker on 23-4-8.
//
#include <sstream>
#include "udp_port.h"
#include "rtc_base/logging.h"
#include "base/socket.h"
#include <rtc_base/crc32.h>
namespace xrtc{

    UDPPort::UDPPort(xrtc::EventLoop *el, const std::string &transport_name, xrtc::IceCandidateComponent component,
                           xrtc::IceParameters ice_params):_el(el),_transport_name(transport_name),_component(component),_ice_params(ice_params){

    }
    UDPPort::~UDPPort() {

    }

    std::string compute_foundation(const std::string& type,const std::string& protocol,
                                   const std::string& relay_protocol,
                                   const rtc::SocketAddress& base){
        std::stringstream ss;
        ss << "type" << base.HostAsURIString() << protocol << relay_protocol;
        return std::to_string(rtc::ComputeCrc32(ss.str()));
    }

    int UDPPort::create_ice_candidate(Network *network,int min_port,int max_port, Candidate &c) {
        _socket = create_udp_socket(network->ip().family());
        if(_socket < 0){
            return -1;
        }
        if(sock_setnonblock(_socket) != 0){
            return -1;
        }
        sockaddr_in addr_in;
        addr_in.sin_family = network->ip().family();
        addr_in.sin_addr = network->ip().ipv4_address();;

        if(sock_bind(_socket,(struct sockaddr*)&addr_in,sizeof(addr_in),min_port,max_port) != 0){
            return -1;
        }
        int port = 0;
        if(sock_get_address(_socket, nullptr,&port) != 0){
            return -1;
        }
        _local_addr.SetIP(network->ip());
        _local_addr.SetPort(port);
        _async_socket = std::make_unique<AsyncUdpSocket>(_el,_socket);

        _async_socket->signal_read_packet.connect(this,&UDPPort::on_read_packet);


        RTC_LOG(LS_INFO) << "prepared socket address: " << _local_addr.ToString();
        c.component = _component;
        c.protocol = "udp";
        c.address = _local_addr;
        c.port = port;
        c.priority = c.get_priority(ICE_TYPE_PREFERENCE_HOST,0,0);
        c.username = _ice_params.ice_ufrag;
        c.password = _ice_params.ice_pwd;
        c.type = LOCAL_PORT_TYPE;
        c.foundation = compute_foundation(c.type,c.protocol,"",c.address);
        _candidates.push_back(c);


        return 0;
    }

    void UDPPort::on_read_packet(AsyncUdpSocket *socket, char *buf, size_t size, const rtc::SocketAddress &addr,
                                 int64_t ts) {
        std::unique_ptr<StunMessage> stun_msg;
        bool res = get_stun_message(buf,size,&stun_msg);
        RTC_LOG(LS_WARNING) << "====res: " << res;
    }

    bool UDPPort::get_stun_message(const char *buf, size_t len, std::unique_ptr<StunMessage> *out_msg) {
        if(!StunMessage::vaildate_fingerprint(buf,len)){
            return false;
        }
        return true;
    }

}
