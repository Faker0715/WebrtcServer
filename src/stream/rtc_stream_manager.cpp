#include <rtc_base/logging.h>

#include "base/conf.h"
#include "stream/push_stream.h"
#include "stream/pull_stream.h"
#include "stream/rtc_stream_manager.h"

extern xrtc::GeneralConf *g_conf;

namespace xrtc {

    RtcStreamManager::RtcStreamManager(EventLoop *el) :
            _el(el),
            _allocator(new PortAllocator()) {
        _allocator->set_port_range(g_conf->ice_min_port, g_conf->ice_max_port);
    }

    RtcStreamManager::~RtcStreamManager() {
    }

    PushStream *RtcStreamManager::_find_push_stream(const std::string &stream_name) {
        auto iter = _push_streams.find(stream_name);
        if (iter != _push_streams.end()) {
            return iter->second;
        }

        return nullptr;
    }

    PullStream *RtcStreamManager::_find_pull_stream(const std::string &stream_name) {
        auto iter = _pull_streams.find(stream_name);
        if (iter != _pull_streams.end()) {
            return iter->second;
        }

        return nullptr;
    }

    void RtcStreamManager::_remove_push_stream(RtcStream *stream) {
        if (!stream) {
            return;
        }

        _remove_push_stream(stream->get_uid(), stream->get_stream_name());
    }

    void RtcStreamManager::_remove_push_stream(uint64_t uid, const std::string &stream_name) {
        PushStream *push_stream = _find_push_stream(stream_name);
        if (push_stream && uid == push_stream->get_uid()) {
            _push_streams.erase(stream_name);
            delete push_stream;
        }
    }

    void RtcStreamManager::_remove_pull_stream(RtcStream *stream) {
        if (!stream) {
            return;
        }

        _remove_pull_stream(stream->get_uid(), stream->get_stream_name());
    }

    void RtcStreamManager::_remove_pull_stream(uint64_t uid, const std::string &stream_name) {
        PullStream *pull_stream = _find_pull_stream(stream_name);
        if (pull_stream && uid == pull_stream->get_uid()) {
            _pull_streams.erase(stream_name);
            delete pull_stream;
        }
    }

    int RtcStreamManager::create_push_stream(uint64_t uid, const std::string &stream_name,
                                             bool audio, bool video,bool is_dtls, uint32_t log_id,
                                             rtc::RTCCertificate *certificate,
                                             std::string &offer) {
        PushStream *stream = _find_push_stream(stream_name);
        if (stream) {
            _push_streams.erase(stream_name);
            delete stream;
        }

        stream = new PushStream(_el, _allocator.get(), uid, stream_name,
                                audio, video, log_id);
        stream->register_listener(this);
        if(is_dtls){
            stream->start(certificate);
        }
        else{
            stream->start(nullptr);
        }

        offer = stream->create_offer();

        _push_streams[stream_name] = stream;
        return 0;
    }

    int RtcStreamManager::create_pull_stream(uint64_t uid, const std::string &stream_name,
                                             bool audio, bool video,bool is_dtls, uint32_t log_id,
                                             rtc::RTCCertificate *certificate,
                                             std::string &offer) {
        PushStream *push_stream = _find_push_stream(stream_name);
        if (!push_stream) {
            RTC_LOG(LS_WARNING) << "Stream not found, uid: " << uid << ", stream_name: "
                                << stream_name << ", log_id: " << log_id;
            return -1;
        }

        _remove_pull_stream(uid, stream_name);

        std::vector<StreamParams> audio_source;
        std::vector<StreamParams> video_source;
        push_stream->get_audio_source(audio_source);
        push_stream->get_video_source(video_source);

        PullStream *stream = new PullStream(_el, _allocator.get(), uid, stream_name,
                                            audio, video, log_id);
        stream->register_listener(this);
        stream->add_audio_source(audio_source);
        stream->add_video_source(video_source);
        if(is_dtls){
            stream->start(certificate);
        }else{
            stream->start(nullptr);
        }
        offer = stream->create_offer();

        _pull_streams[stream_name] = stream;
        return 0;
    }

    int RtcStreamManager::set_answer(uint64_t uid, const std::string &stream_name,
                                     const std::string &answer, const std::string &stream_type,
                                     uint32_t log_id) {
        if ("push" == stream_type) {
            PushStream *push_stream = _find_push_stream(stream_name);
            if (!push_stream) {
                RTC_LOG(LS_WARNING) << "push stream not found, uid: " << uid
                                    << ", stream_name: " << stream_name
                                    << ", log_id: " << log_id;
                return -1;
            }

            if (uid != push_stream->uid) {
                RTC_LOG(LS_WARNING) << "uid invalid, uid: " << uid
                                    << ", stream_name: " << stream_name
                                    << ", log_id: " << log_id;
                return -1;
            }

            push_stream->set_remote_sdp(answer);

        } else if ("pull" == stream_type) {
            PullStream *pull_stream = _find_pull_stream(stream_name);
            if (!pull_stream) {
                RTC_LOG(LS_WARNING) << "pull stream not found, uid: " << uid
                                    << ", stream_name: " << stream_name
                                    << ", log_id: " << log_id;
                return -1;
            }

            if (uid != pull_stream->uid) {
                RTC_LOG(LS_WARNING) << "uid invalid, uid: " << uid
                                    << ", stream_name: " << stream_name
                                    << ", log_id: " << log_id;
                return -1;
            }

            pull_stream->set_remote_sdp(answer);
        }

        return 0;
    }

    int RtcStreamManager::stop_push(uint64_t uid, const std::string &stream_name) {
        _remove_push_stream(uid, stream_name);
        return 0;
    }

    int RtcStreamManager::stop_pull(uint64_t uid, const std::string &stream_name) {
        _remove_pull_stream(uid, stream_name);
        return 0;
    }

    void RtcStreamManager::on_connection_state(RtcStream *stream,
                                               PeerConnectionState state) {
        if (state == PeerConnectionState::k_failed) {
            if (stream->stream_type() == RtcStreamType::k_push) {
                _remove_push_stream(stream);
            }else if(stream->stream_type() == RtcStreamType::k_pull){
                _remove_pull_stream(stream);
            }
        }
    }

    void RtcStreamManager::on_rtp_packet_received(RtcStream *stream,
                                                  const char *data, size_t len) {
        if (RtcStreamType::k_push == stream->stream_type()) {
            PullStream *pull_stream = _find_pull_stream(stream->get_stream_name());
            if (pull_stream) {
                pull_stream->send_rtp(data, len);
            }
        }
    }

    void RtcStreamManager::on_rtcp_packet_received(RtcStream *stream,
                                                   const char *data, size_t len) {
        if (RtcStreamType::k_push == stream->stream_type()) {
            PullStream *pull_stream = _find_pull_stream(stream->get_stream_name());
            if (pull_stream) {
                pull_stream->send_rtcp(data, len);
            }
        } else if (RtcStreamType::k_pull == stream->stream_type()) {
            PushStream *push_stream = _find_push_stream(stream->get_stream_name());
            if (push_stream) {
                push_stream->send_rtcp(data, len);
            }
        }

    }

    void RtcStreamManager::on_stream_exception(RtcStream *stream) {
        if (RtcStreamType::k_push == stream->stream_type()) {
            _remove_push_stream(stream);
        } else if (RtcStreamType::k_pull == stream->stream_type()) {
            _remove_pull_stream(stream);
        }

    }

} // namespace xrtc


