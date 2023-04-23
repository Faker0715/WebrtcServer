//
// Created by faker on 23-4-8.
//

#ifndef XRTCSERVER_UDP_PORT_H
#define XRTCSERVER_UDP_PORT_H

#include <string>
#include <memory>
#include <map>
#include "base/event_loop.h"
#include "ice_def.h"
#include "ice_credentials.h"
#include "base/network.h"
#include "candidate.h"
#include "rtc_base/socket_address.h"
#include "base/async_udp_socket.h"
#include "stun.h"
#include "ice_connection.h"

namespace xrtc {

    class IceConnection;
    typedef std::map<rtc::SocketAddress,IceConnection*> AddressMap;
    class UDPPort : public sigslot::has_slots<> {

    public:
        UDPPort(EventLoop *el, const std::string &transport_name, IceCandidateComponent component,
                IceParameters ice_params);

        ~UDPPort();
        std::string ice_ufrag() const { return _ice_params.ice_ufrag; }
        std::string ice_pwd() const { return _ice_params.ice_pwd; }
        const std::string transport_name() const { return _transport_name; }
        IceCandidateComponent component() const { return _component; }
        const rtc::SocketAddress& local_addr() {return _local_addr;}
        int create_ice_candidate(Network *network, int min_port, int max_port, Candidate &c);
        void create_stun_username(const std::string &remote_username, std::string *stun_attr_username);
        bool get_stun_message(const char* data,size_t len,const rtc::SocketAddress& addr,
                              std::unique_ptr<StunMessage>* out_msg,
                              std::string* out_username);

        std::string to_string();

        sigslot::signal4<UDPPort*,const rtc::SocketAddress&,StunMessage*,const std::string&> signal_unknown_address;

        void send_binding_error_response(StunMessage* stun_msg,const rtc::SocketAddress& addr,
                                                  int err_code, const std::string& reason);
        IceConnection* create_connection(const Candidate& candidate);
        int send_to(const char* buf,size_t len,const rtc::SocketAddress& addr);

        const std::vector<Candidate>& candidates() {
            return _candidates;
        }


    private:
        void _on_read_packet(AsyncUdpSocket *socket, char *buf, size_t size, const rtc::SocketAddress &addr, int64_t ts);
        bool _parse_stun_username(StunMessage *stun_msg, std::string *local_ufrag,std::string* remote_ufrag);
        IceConnection* get_connection(const rtc::SocketAddress &addr);
    private:
        EventLoop *_el;
        std::string _transport_name;
        IceCandidateComponent _component;
        IceParameters _ice_params;
        int _socket = -1;
        rtc::SocketAddress _local_addr;
        std::unique_ptr<AsyncUdpSocket> _async_socket;
        std::vector<Candidate> _candidates;
        AddressMap _connections;

    };
}


#endif //XRTCSERVER_UDP_PORT_H
