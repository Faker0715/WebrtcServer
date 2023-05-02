#include <rtc_base/logging.h>

#include "stream/rtc_stream.h"

namespace xrtc {

    RtcStream::RtcStream(EventLoop *el, PortAllocator *allocator,
                         uint64_t uid, const std::string &stream_name,
                         bool audio, bool video, uint32_t log_id) :
            el(el), uid(uid), stream_name(stream_name), audio(audio),
            video(video), log_id(log_id),
            pc(new PeerConnection(el, allocator)) {
        pc->signal_connection_state.connect(this, &RtcStream::_on_connection_state);
        pc->signal_rtp_packet_received.connect(this, &RtcStream::_on_rtp_packet_received);
        pc->signal_rtcp_packet_received.connect(this, &RtcStream::_on_rtcp_packet_received);
    }

    RtcStream::~RtcStream() {
        pc->destroy();
    }

    void RtcStream::_on_connection_state(PeerConnection *, PeerConnectionState state) {
        if (_state == state) {
            return;
        }

        RTC_LOG(LS_INFO) << to_string() << ": PeerConnectionState change from " << _state
                         << " to " << state;
        _state = state;

        if (_listener) {
            _listener->on_connection_state(this, state);
        }
    }

    void RtcStream::_on_rtp_packet_received(PeerConnection *,
                                            rtc::CopyOnWriteBuffer *packet, int64_t /*ts*/) {
        if (_listener) {
            _listener->on_rtp_packet_received(this, (const char *) packet->data(), packet->size());
        }
    }

    void RtcStream::_on_rtcp_packet_received(PeerConnection *,
                                             rtc::CopyOnWriteBuffer *packet, int64_t /*ts*/) {
        if (_listener) {
            _listener->on_rtcp_packet_received(this, (const char *) packet->data(), packet->size());
        }
    }


    int RtcStream::start(rtc::RTCCertificate *certificate) {
        return pc->init(certificate);
    }

    int RtcStream::set_remote_sdp(const std::string &sdp) {
        return pc->set_remote_sdp(sdp);
    }

    int RtcStream::send_rtp(const char *data, size_t len) {
        if (pc) {
            return pc->send_rtp(data, len);
        }
        return -1;
    }

    std::string RtcStream::to_string() {
        std::stringstream ss;
        ss << "Stream[" << this << "|" << uid << "|" << stream_name << "]";
        return ss.str();
    }

    int RtcStream::send_rtcp(const char *data, size_t len) {
        if(pc){
            return pc->send_rtcp(data, len);
        }
        return -1;
    }

} // namespace xrtc


