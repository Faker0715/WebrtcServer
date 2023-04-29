//
// Created by faker on 23-4-2.
//

#include "rtc_stream_manager.h"
#include "rtc_stream.h"
#include "push_stream.h"
#include "base/conf.h"
#include <rtc_base/logging.h>
extern xrtc::GeneralConf* g_conf;
namespace xrtc {
    xrtc::RtcStreamManager::RtcStreamManager(xrtc::EventLoop *el) :
            _el(el),
            _allocator(new PortAllocator()){
        _allocator->set_port_range(g_conf->ice_min_port,g_conf->ice_max_port);
    }

    RtcStreamManager::~RtcStreamManager() {

    }

    int RtcStreamManager::create_push_stream(uint64_t uid, const std::string &stream_name, bool audio, bool video, uint32_t log_id, rtc::RTCCertificate* certificate, std::string &offer) {
        PushStream *stream = _find_push_stream(stream_name);
        if(stream){
            _push_streams.erase(stream_name);
            delete stream;
        }
        stream = new PushStream(_el,_allocator.get(), uid, stream_name, audio, video, log_id);
        stream->register_listener(this);
        stream->start(certificate);
        offer = stream->create_offer();
        _push_streams[stream_name] = stream;
        return 0;
    }

    PushStream* RtcStreamManager::_find_push_stream(const std::string &stream_name) {
        auto iter = _push_streams.find(stream_name);
        if (iter != _push_streams.end()) {
            return iter->second;
        }
        return nullptr;
    }

    int RtcStreamManager::set_answer(uint64_t uid, const std::string &stream_name, const std::string &answer,
                                     const std::string &stream_type, uint32_t log_id) {
        if("push" == stream_type){
            PushStream* push_stream = _find_push_stream(stream_name);
            if(!push_stream){
                RTC_LOG(LS_WARNING) << "push stream not found, uid: " << uid
                    << ", stream_name: " << stream_name << ", log_id: " << log_id;
                return -1;
            }
            if(uid != push_stream->uid){
                RTC_LOG(LS_WARNING) << "uid invalid, uid: " << uid
                    << ", stream_name: " << stream_name << ", log_id: " << log_id;
                return -1;
            }
            push_stream->set_remote_sdp(answer);
        }else if("pull" == stream_type){
            PullStream* pull_stream = _find_pull_stream(stream_name);
            if(!pull_stream){
                RTC_LOG(LS_WARNING) << "pull stream not found, uid: " << uid
                                    << ", stream_name: " << stream_name << ", log_id: " << log_id;
                return -1;
            }
            if(uid != pull_stream->uid){
                RTC_LOG(LS_WARNING) << "uid invalid, uid: " << uid
                                    << ", stream_name: " << stream_name << ", log_id: " << log_id;
                return -1;
            }
            pull_stream->set_remote_sdp(answer);
        }
        return 0;
    }

    void RtcStreamManager::on_connection_state(RtcStream *stream, PeerConnectionState state) {
        if(state == PeerConnectionState::k_failed){
//           区分pull和pushstream
            if(stream->stream_type() == RtcStreamType::k_push){
                _remove_push_stream(stream);
            }
        }
    }

    void RtcStreamManager::_remove_push_stream(RtcStream *stream) {
        if(!stream){
            return;
        }
        _remove_push_stream(stream->get_uid(),stream->get_stream_name());
    }
    void RtcStreamManager::_remove_pull_stream(RtcStream *stream) {
        if(!stream){
            return;
        }
        _remove_pull_stream(stream->get_uid(),stream->get_stream_name());
    }
    void RtcStreamManager::_remove_push_stream(uint64_t uid,const std::string& stream_name) {
        PushStream* push_stream = _find_push_stream(stream_name);
        if(push_stream && uid == push_stream->get_uid()){
            _push_streams.erase(stream_name);
            delete push_stream;
        }
    }
    void RtcStreamManager::_remove_pull_stream(uint64_t uid,const std::string& stream_name) {
        PullStream* pull_stream = _find_pull_stream(stream_name);
        if(pull_stream && uid == pull_stream->get_uid()){
            _pull_streams.erase(stream_name);
            delete pull_stream;
        }
    }
    int RtcStreamManager::stop_push(uint64_t uid, const std::string &stream_name) {
        _remove_push_stream(uid,stream_name);
        return 0;
    }

    int RtcStreamManager::create_pull_stream(uint64_t uid, const std::string& stream_name,
                                             bool audio, bool video, uint32_t log_id,
                                             rtc::RTCCertificate* certificate,
                                             std::string& offer)
    {
        PushStream* push_stream = _find_push_stream(stream_name);
        if (!push_stream) {
            RTC_LOG(LS_WARNING) << "Stream not found, uid: " << uid << ", stream_name: "
                                << stream_name << ", log_id: " << log_id;
            return -1;
        }
        // 防止重复拉
        _remove_pull_stream(uid, stream_name);

        std::vector<StreamParams> audio_source;
        std::vector<StreamParams> video_source;
        push_stream->get_audio_source(audio_source);
        push_stream->get_video_source(video_source);




        PullStream* stream = new PullStream(_el, _allocator.get(), uid, stream_name,
                                            audio, video, log_id);
        stream->register_listener(this);
        stream->add_audio_source(audio_source);
        stream->add_video_source(video_source);
        stream->start(certificate);
        offer = stream->create_offer();

        _pull_streams[stream_name] = stream;
        return 0;
    }


    PullStream *RtcStreamManager::_find_pull_stream(const std::string &stream_name) {
        auto iter = _pull_streams.find(stream_name);
        if (iter != _pull_streams.end()) {
            return iter->second;
        }
        return nullptr;
    }

}
