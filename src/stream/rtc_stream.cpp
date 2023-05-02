#include <rtc_base/logging.h>

#include "stream/rtc_stream.h"

namespace xrtc {

    const size_t k_ice_timeout = 30000;
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
        if (_ice_timeout_watcher) {
            el->delete_timer(_ice_timeout_watcher);
            _ice_timeout_watcher = nullptr;
        }
        pc->destroy();
    }

    void RtcStream::_on_connection_state(PeerConnection *, PeerConnectionState state) {
        if (_state == state) {
            return;
        }

        RTC_LOG(LS_INFO) << to_string() << ": PeerConnectionState change from " << _state
                         << " to " << state;
        _state = state;
        if (_state == PeerConnectionState::k_connected) {
            if (_ice_timeout_watcher) {
                el->delete_timer(_ice_timeout_watcher);
                _ice_timeout_watcher = nullptr;
            }
        }

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

    void ice_timeout_cb(EventLoop */*el*/, TimerWatcher */*w*/, void *data) {
        RtcStream *stream = (RtcStream *) data;
        if (stream->_state != PeerConnectionState::k_connected) {
            if(stream->_listener){
                stream->_listener->on_stream_exception(stream);
            }
        }
    }

    int RtcStream::start(rtc::RTCCertificate *certificate) {
        _ice_timeout_watcher = el->create_timer(ice_timeout_cb, this, false);
        el->start_timer(_ice_timeout_watcher, k_ice_timeout * 1000);
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
        if (pc) {
            return pc->send_rtcp(data, len);
        }
        return -1;
    }

} // namespace xrtc


