//
// Created by faker on 23-4-8.
//

#ifndef XRTCSERVER_UDP_PORT_H
#define XRTCSERVER_UDP_PORT_H

#include <string>
#include <memory>
#include "base/event_loop.h"
#include "ice_def.h"
#include "ice_credentials.h"
#include "base/network.h"
#include "candidate.h"
#include "rtc_base/socket_address.h"
#include "base/async_udp_socket.h"
#include "stun.h"

namespace xrtc {

    class UDPPort : public sigslot::has_slots<> {

    public:
        UDPPort(EventLoop *el, const std::string &transport_name, IceCandidateComponent component,
                IceParameters ice_params);

        ~UDPPort();

        int create_ice_candidate(Network *network, int min_port, int max_port, Candidate &c);

        bool get_stun_message(const char* data,size_t len,const rtc::SocketAddress& addr,
                              std::unique_ptr<StunMessage>* out_msg,
                              std::string* out_username);

        std::string to_string();

        sigslot::signal4<UDPPort*,const rtc::SocketAddress&,StunMessage*,const std::string&> signal_unknown_address;

        void send_binding_error_response(StunMessage* stun_msg,const rtc::SocketAddress& addr,
                                                  int err_code, const std::string& reason);
    private:

        void on_read_packet(AsyncUdpSocket *socket, char *buf, size_t size, const rtc::SocketAddress &addr, int64_t ts);
        bool _parse_stun_username(StunMessage *stun_msg, std::string *local_ufrag,std::string* remote_ufrag);
    private:
        EventLoop *_el;
        std::string _transport_name;
        IceCandidateComponent _component;
        IceParameters _ice_params;
        int _socket = -1;
        rtc::SocketAddress _local_addr;
        std::unique_ptr<AsyncUdpSocket> _async_socket;
        std::vector<Candidate> _candidates;
    };
}


#endif //XRTCSERVER_UDP_PORT_H
