//
// Created by faker on 23-4-4.
//

#ifndef XRTCSERVER_ICE_TRANSPORT_CHANNEL_H
#define XRTCSERVER_ICE_TRANSPORT_CHANNEL_H

#include <string>
#include <vector>
#include "ice_def.h"
#include "base/event_loop.h"
#include "port_allocator.h"
#include "ice_credentials.h"
#include "candidate.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "udp_port.h"
#include "ice_controller.h"


namespace xrtc {
    class UDPPort;

    enum class IceTransportState{
        k_new,
        k_checking,
        k_connected,
        k_completed,
        k_failed,
        k_disconnected,
        k_closed
    };
    class IceController;

    class IceTransportChannel : public sigslot::has_slots<> {
    public:
        IceTransportChannel(EventLoop *el, PortAllocator *allocator, const std::string &transport_name,
                            IceCandidateComponent component);

        virtual ~IceTransportChannel();

        const std::string &transport_name() const { return _transport_name; }

        IceCandidateComponent component() const { return _component; }

        void set_ice_params(const IceParameters &ice_params);

        void set_remote_ice_params(const IceParameters &ice_params);

        void gathering_candidate();

        std::string to_string();

        int send_packet(const char *data, size_t len);


        void _on_check_and_ping();

        friend void ice_ping_cb(EventLoop *, TimerWatcher *, void *data);

        void _ping_connection(IceConnection *conn);

        void _on_connection_state_change(IceConnection *);
        bool receiving() { return _receiving; }

        void _maybe_switch_selected_connection(IceConnection *conn);

        void _on_connection_destroy(IceConnection *conn);

        void _switch_selected_connection(IceConnection *conn);

        void _update_state();

        void _set_receiving(bool receiving);

        void _set_writable(bool writable);

        bool writable() { return _writable; }

        IceTransportState state() { return _state; }

        sigslot::signal2<IceTransportChannel *, const std::vector<Candidate> &> signal_candidate_allocate_done;
        sigslot::signal1<IceTransportChannel *> signal_receiving_state;
        sigslot::signal1<IceTransportChannel *> signal_writable_state;
        sigslot::signal1<IceTransportChannel *> signal_ice_state_changed;
        sigslot::signal4<IceTransportChannel *, const char *, size_t, int64_t> signal_read_packet;
    private:
        void _on_unknown_address(xrtc::UDPPort *port, const rtc::SocketAddress &addr, xrtc::StunMessage *msg,
                                 const std::string &remote_ufrag);

        void _sort_connections_and_update_state();

        void _maybe_start_pinging();

        void _add_connection(IceConnection *conn);

        void _update_connection_states();

        void _on_read_packet(IceConnection *, const char *buf, size_t len, int64_t ts);

        IceTransportState _compute_ice_transport_state();
    private:

        EventLoop *_el;
        std::string _transport_name;
        IceCandidateComponent _component;
        PortAllocator *_allocator;
        IceParameters _ice_params;
        IceParameters _remote_ice_params;
        std::vector<Candidate> _local_candidates;
        std::unique_ptr<IceController> _ice_controller;
        bool _start_pinging = false;
        TimerWatcher *_ping_watcher = nullptr;
        int64_t _last_ping_sent_ms = 0;
        int _cur_ping_interval = WEAK_PING_INTERVAL;

        IceConnection *_selected_connection = nullptr;
        bool _receiving = false;
        bool _writable = false;

        IceTransportState _state = IceTransportState::k_new;

        bool _had_connection = false;
        bool _has_been_connection = false; // 连接是否连通过

        std::vector<UDPPort*> _ports;
    };
}


#endif //XRTCSERVER_ICE_TRANSPORT_CHANNEL_H
