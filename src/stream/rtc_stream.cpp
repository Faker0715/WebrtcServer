//
// Created by faker on 23-4-2.
//

#include "rtc_stream.h"
#include <rtc_base/logging.h>

namespace xrtc {
    RtcStream::RtcStream(EventLoop *el, PortAllocator* allocator, uint64_t uid, const std::string &stream_name, bool audiom, bool video,
                               uint32_t log_id) : el(el), uid(uid), stream_name(stream_name), audio(audiom),
                                                  video(video),
                                                  log_id(log_id), pc(new PeerConnection(el,allocator)) {
        pc->signal_connection_state.connect(this,&RtcStream::_on_connection_state);
        pc->signal_rtp_packet_received.connect(this,&RtcStream::_on_rtp_packet_received);
        pc->signal_rtcp_packet_received.connect(this,&RtcStream::_on_rtcp_packet_received);

    }
    void RtcStream::_on_rtp_packet_received(PeerConnection*,rtc::CopyOnWriteBuffer* packet,int64_t /*ts*/){
        if(_listener){
            _listener->on_rtp_packet_received(this,(const char*)packet->data(),packet->size());
        }
    }
    void RtcStream::_on_rtcp_packet_received(PeerConnection*,rtc::CopyOnWriteBuffer* packet,int64_t /*ts*/){
        if(_listener){
            _listener->on_rtcp_packet_received(this,(const char*)packet->data(),packet->size());
        }
    }
    void RtcStream::_on_connection_state(PeerConnection*,PeerConnectionState state){
        if(_state == state){
            return;
        }
        RTC_LOG(LS_INFO) << to_string() << "PeerConnectionState change from " << _state << " to" << state;
        _state = state;
        if(_listener){
            _listener->on_connection_state(this,state);
        }

    }

    RtcStream::~RtcStream() {
        pc->destroy();
    }

    int RtcStream::start(rtc::RTCCertificate *certificate) {
        return pc->init(certificate);
    }

    int RtcStream::set_remote_sdp(const std::string &sdp) {
        return pc->set_remote_sdp(sdp);
    }

    std::string RtcStream::to_string() {
        std::stringstream ss;
        ss << "Stream[" << this << "|" << uid << "|" << stream_name << "}";
        return ss.str();
    }
}