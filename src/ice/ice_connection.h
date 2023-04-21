//
// Created by faker on 23-4-19.
//

#ifndef XRTCSERVER_ICE_CONNECTION_H
#define XRTCSERVER_ICE_CONNECTION_H

#include "base/event_loop.h"
#include "udp_port.h"
#include "candidate.h"

namespace xrtc {
    class UDPPort;
    class IceConnection {
    public:
        enum WriteState{
            STATE_WRITABLE,
            STATE_WRITE_UNRELIABLE,
            STATE_WRITE_INIT,
            STATE_WRITE_TIMEOUT,
        };
        IceConnection(EventLoop *el, UDPPort *port, const Candidate &remote_candidate);

        ~IceConnection();
        const Candidate& remote_candidate() const { return _remote_candidate; }
        void on_read_packet(const char* buf, size_t len,int64_t ts);
        void handle_stun_binding_request(StunMessage* stun_msg);
        void send_stun_binding_response(StunMessage* stun_msg);
        void send_response_message(const StunMessage &msg);
        bool maybe_set_remote_ice_params(const IceParameters& ice_params);
        bool writable() const { return _write_state == STATE_WRITABLE; }
        bool receving(){return _receiving;}
        bool weak(){
            !(writable() && receving());
        }
        bool active(){
            _write_state != STATE_WRITE_TIMEOUT;
        }
        bool stable(int64_t now) const;

        std::string to_string();
        int64_t last_ping_sent() const { return _last_ping_sent; }
        int num_pings_sent() const {
            return _num_pings_sent;
        }
    private:
        EventLoop* _el;
        UDPPort* _port;
        Candidate _remote_candidate;
        WriteState _write_state = STATE_WRITE_INIT;
        bool _receiving = false;
        int64_t _last_ping_sent = 0;
        int _num_pings_sent = 0;

    };
}


#endif //XRTCSERVER_ICE_CONNECTION_H
