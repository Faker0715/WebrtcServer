//
// Created by faker on 23-4-4.
//

#ifndef XRTCSERVER_TRANSPORT_CONTROLLER_H
#define XRTCSERVER_TRANSPORT_CONTROLLER_H

#include "base/event_loop.h"
#include "ice/ice_agent.h"
#include "session_description.h"
#include "ice/port_allocator.h"
#include "dtls_transport.h"
#include "pc/peer_connection_def.h"
namespace xrtc{
    enum class DtlsTransportState;
    class TransportController : public sigslot::has_slots<>{
    public:
        TransportController(EventLoop* el,PortAllocator* allocator);
        ~TransportController();
        int set_local_description(SessionDescription* desc);
        int set_remote_description(SessionDescription* desc);
        void set_local_certificate(rtc::RTCCertificate* cert);
        DtlsTransport *_get_dtls_transport(const std::string &transport_name);
        sigslot::signal4<TransportController*, const std::string &, IceCandidateComponent, const std::vector<Candidate> &> signal_candidate_allocate_done;
        sigslot::signal2<TransportController*, PeerConnectionState> signal_connection_state;
    private:
        void on_candidate_allocate_done(IceAgent* agent,const std::string& transport_name,IceCandidateComponent component,const std::vector<Candidate>& candidates);
        void _add_dtls_transport(DtlsTransport* dtls);
        void _on_dtls_receiving_state(DtlsTransport*);

        void _on_dtls_writable_state(DtlsTransport*);
        void _on_dtls_state(DtlsTransport*,DtlsTransportState);
        void _update_state();
    private:
        EventLoop* _el;
        IceAgent* _ice_agent;
        std::map<std::string,DtlsTransport*> _dtls_transport_by_name;
        rtc::RTCCertificate* _local_certificate = nullptr;
        PeerConnectionState _pc_state = PeerConnectionState::k_new;

    };
}


#endif //XRTCSERVER_TRANSPORT_CONTROLLER_H
