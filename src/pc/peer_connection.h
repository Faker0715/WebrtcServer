//
// Created by faker on 23-4-2.
//

#ifndef XRTCSERVER_PEER_CONNECTION_H
#define XRTCSERVER_PEER_CONNECTION_H

#include <string>
#include <memory>
#include "base/event_loop.h"
#include "session_description.h"
#include <rtc_base/rtc_certificate.h>
#include "transport_controller.h"
#include "ice/port_allocator.h"

namespace xrtc{
    struct RTCOfferAnswerOptions{
        bool send_audio = true;
        bool send_video = true;
        bool recv_audio = true;
        bool recv_video = true;
        bool use_rtp_mux = true;
        bool use_rtcp_mux = true;
        bool dtls_on = true;
    };
    class PeerConnection : public sigslot::has_slots<>{
    public:
        PeerConnection(EventLoop* el,PortAllocator* allocator);
        ~PeerConnection();
        int init(rtc::RTCCertificate* certificate);
        std::string create_offer(const RTCOfferAnswerOptions options);
        int set_remote_sdp(const std::string& sdp);
        sigslot::signal2<PeerConnection*,PeerConnectionState> signal_connection_state;
    private:
        void _on_candidate_allocate_done(TransportController* controller,const std::string& transport_name,IceCandidateComponent component,const std::vector<Candidate>& candidates);
        void _on_connection_state(TransportController *, PeerConnectionState state);
    private:
        EventLoop* _el;
        rtc::RTCCertificate* _certificate = nullptr;
        std::unique_ptr<SessionDescription> _local_desc;
        std::unique_ptr<SessionDescription> _remote_desc;
        std::unique_ptr<TransportController> _transport_controller;

    };
}


#endif //XRTCSERVER_PEER_CONNECTION_H
