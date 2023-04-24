//
// Created by faker on 23-4-19.
//

#ifndef XRTCSERVER_ICE_CONNECTION_H
#define XRTCSERVER_ICE_CONNECTION_H

#include "base/event_loop.h"
#include "udp_port.h"
#include "candidate.h"
#include "stun_request.h"

namespace xrtc {
    class UDPPort;
    class IceConnection;
    class ConnectionRequest: public StunRequest{
    public:
        ConnectionRequest(IceConnection* conn);
    protected:
        void prepare(StunMessage *msg) override;
        void on_request_response(StunMessage *msg) override;
        void on_error_request_response(StunMessage*) override;

    private:
        IceConnection* _connection;
    };
    class IceConnection: public sigslot::has_slots<>  {
    public:
        enum WriteState{
            STATE_WRITABLE,
            STATE_WRITE_UNRELIABLE,
            STATE_WRITE_INIT,
            STATE_WRITE_TIMEOUT,
        };
        struct SentPing{
            SentPing(const std::string& id,int64_t ts):id(id),sent_time(ts){}
            std::string id;
            int64_t sent_time;
        };
        IceConnection(EventLoop *el, UDPPort *port, const Candidate &remote_candidate);

        ~IceConnection();
        const Candidate& remote_candidate() const { return _remote_candidate; }
        const Candidate& local_candidate() const;

        UDPPort* port() const { return _port; }

        void on_read_packet(const char* buf, size_t len,int64_t ts);
        void handle_stun_binding_request(StunMessage* stun_msg);
        void send_stun_binding_response(StunMessage* stun_msg);
        void send_response_message(const StunMessage &msg);
        void maybe_set_remote_ice_params(const IceParameters& ice_params);
        bool writable() const { return _write_state == STATE_WRITABLE; }
        bool receiving(){return _receiving;}
        bool weak(){
            !(writable() && receiving());
        }
        bool active(){
            return _write_state != STATE_WRITE_TIMEOUT;
        }
        bool stable(int64_t now) const;
        void ping(int64_t now);
        void received_ping_response(int rtt);
        void update_receiving(int64_t now);
        void fail_and_destory();
        int receiving_timeout();
        WriteState write_state(){
            return _write_state;
        };

        int rtt() {
            return _rtt;
        }
        uint64_t  priority();

        std::string to_string();
        int64_t last_ping_sent() const { return _last_ping_sent; }
        int64_t last_received();

        void set_selected(bool selected) { _selected = selected; }
        bool selected() const { return _selected; }
        int num_pings_sent() const {
            return _num_pings_sent;
        }
        void on_connection_request_response(ConnectionRequest* request,StunMessage* msg) ;
        void on_connection_error_request_response(ConnectionRequest* request,StunMessage* msg) ;

        void set_write_state(WriteState state);
        void print_pings_since_last_response(std::string& pings,size_t max);

        sigslot::signal1<IceConnection*> signal_state_change;

    private:
        void _on_stun_send_packet(StunRequest* request,const char* buf,size_t len);
    private:
        EventLoop* _el;
        UDPPort* _port;
        Candidate _remote_candidate;
        WriteState _write_state = STATE_WRITE_INIT;
        bool _receiving = false;
        int64_t _last_ping_sent = 0;
        int64_t _last_ping_received = 0;
        int64_t _last_ping_response_received = 0;
        int64_t _last_data_received = 0;
        int _num_pings_sent = 0;
        int _rtt = 3000;
        int _rtt_samples = 0;
        std::vector<SentPing> _pings_since_last_response;
        StunRequestManager _requests;
        bool _selected = false;
    };
}


#endif //XRTCSERVER_ICE_CONNECTION_H
