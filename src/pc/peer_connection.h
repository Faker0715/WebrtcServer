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
        int init(rtc::RTCCertificate* certificate);
        std::string create_offer(const RTCOfferAnswerOptions options);
        int set_remote_sdp(const std::string& sdp);
        void destroy();
        SessionDescription* remote_desc(){
            return _remote_desc.get();
        }

        SessionDescription* local_desc(){
            return _local_desc.get();
        }
        void add_audio_source(const std::vector<StreamParams>& source){
            _audio_source = source;
        }
        void add_video_source(const std::vector<StreamParams>& source){
            _video_source = source;
        }

        sigslot::signal2<PeerConnection*,PeerConnectionState> signal_connection_state;
        sigslot::signal3<PeerConnection*,rtc::CopyOnWriteBuffer*,int64_t> signal_rtp_packet_received;
        sigslot::signal3<PeerConnection*,rtc::CopyOnWriteBuffer*,int64_t> signal_rtcp_packet_received;
    private:
        // 只能通过destory进行销毁
        ~PeerConnection();
        void _on_candidate_allocate_done(TransportController* controller,const std::string& transport_name,IceCandidateComponent component,const std::vector<Candidate>& candidates);
        void _on_connection_state(TransportController *, PeerConnectionState state);
        void _on_rtp_packet_received(TransportController *controller,rtc::CopyOnWriteBuffer* packet,int64_t ts);
        void _on_rtcp_packet_received(TransportController *controller,rtc::CopyOnWriteBuffer* packet,int64_t ts);
        friend void destroy_timer_cb(EventLoop* el,TimerWatcher* w,void* data);
    private:
        EventLoop* _el;
        rtc::RTCCertificate* _certificate = nullptr;
        std::unique_ptr<SessionDescription> _local_desc;
        std::unique_ptr<SessionDescription> _remote_desc;
        std::unique_ptr<TransportController> _transport_controller;
        TimerWatcher* _destroy_timer = nullptr;
        std::vector<StreamParams> _audio_source;
        std::vector<StreamParams> _video_source;

    };
}


#endif //XRTCSERVER_PEER_CONNECTION_H
