//
// Created by faker on 23-4-2.
//

#include "peer_connection.h"
#include "session_description.h"
#include "ice/ice_credentials.h"
#include "rtc_base/logging.h"
#include "absl/algorithm/container.h"

namespace xrtc {
    static RtpDirection get_direction(bool send, bool recv) {
        if (send & recv) {
            return RtpDirection::k_send_recv;
        } else if (send && !recv) {
            return RtpDirection::k_send_only;
        } else if (!send && recv) {
            return RtpDirection::k_recv_only;
        } else {
            return RtpDirection::k_inactive;
        }
    }

    PeerConnection::PeerConnection(EventLoop *el, PortAllocator *allocator) : _el(el), _transport_controller(
            new TransportController(el, allocator)) {
        _transport_controller->signal_candidate_allocate_done.connect(this,
                                                                      &PeerConnection::_on_candidate_allocate_done);
        _transport_controller->signal_connection_state.connect(this,
                                                       &PeerConnection::_on_connection_state);


    }
    void PeerConnection::_on_connection_state(TransportController*,PeerConnectionState state){
        signal_connection_state(this,state);
    }

    PeerConnection::~PeerConnection() {
        if(_destroy_timer){
            _el->delete_timer(_destroy_timer);
            _destroy_timer = nullptr;
        }
        RTC_LOG(LS_INFO) << "PeerConnection destroy";

    }

    std::string PeerConnection::create_offer(const RTCOfferAnswerOptions options) {
        if (options.dtls_on && !_certificate) {
            RTC_LOG(LS_WARNING) << "certificate is null";
            return "";
        }
        _local_desc = std::make_unique<SessionDescription>(SdpType::k_offer);

        IceParameters ice_param = IceCredentials::create_random_ice_credentials();


        if (options.recv_audio) {
            auto audio = std::make_shared<AudioContentDescription>();
            audio->set_direction(get_direction(options.send_audio, options.recv_audio));
            audio->set_rtcp_mux(options.use_rtcp_mux);
            _local_desc->add_content(audio);
            _local_desc->add_transport_info(audio->mid(), ice_param, _certificate);
        }
        if (options.recv_video) {
            auto video = std::make_shared<VideoContentDescription>();
            video->set_direction(get_direction(options.send_video, options.recv_video));
            video->set_rtcp_mux(options.use_rtcp_mux);
            _local_desc->add_content(video);
            _local_desc->add_transport_info(video->mid(), ice_param, _certificate);
        }
        if (options.use_rtp_mux) {
            ContentGroup offer_bundle("BUNDLE");
            for (auto &content: _local_desc->contents()) {
                offer_bundle.add_content_name(content->mid());
            }
            if (!offer_bundle.contents_names().empty()) {
                _local_desc->add_group(offer_bundle);
            }
        }
        _transport_controller->set_local_description(_local_desc.get());
        return _local_desc->to_string();
    }

    int PeerConnection::init(rtc::RTCCertificate *certificate) {
        _certificate = certificate;
        _transport_controller->set_local_certificate(certificate);
        return 0;
    }

    void PeerConnection::_on_candidate_allocate_done(TransportController */*controller*/, const std::string &transport_name,
                                                    IceCandidateComponent /*component*/,
                                                    const std::vector<Candidate> &candidates) {
        for (auto c: candidates) {
            RTC_LOG(LS_INFO) << "candidate gathered, transport_name:" << transport_name << ", " << c.to_string();
        }
        if (!_local_desc) {
            return;
        }
        auto content = _local_desc->get_content(transport_name);
        if (content)
            content->add_candidates(candidates);

    }


    static std::string get_attribute(
            const std::string &line, bool is_rn) {
        std::vector<std::string> fields;
        size_t size = rtc::tokenize(line, ':', &fields);
        if (size != 2) {
            RTC_LOG(LS_WARNING) << "get attribute error: " << line;
            return "";
        }
        if (is_rn) {
            return fields[1].substr(0, fields[1].length() - 1);
        }
        return fields[1];

    }


    static int parse_transport_info(TransportDescription *td,
                                    const std::string &line,
                                    bool is_rn) {
        if (line.find("a=ice-ufrag") != std::string::npos) {
            td->ice_ufrag = get_attribute(line, is_rn);
            if (td->ice_ufrag.empty()) {
                return -1;
            }
        } else if (line.find("a=ice-pwd") != std::string::npos) {
            td->ice_pwd = get_attribute(line, is_rn);
            if (td->ice_pwd.empty()) {
                return -1;
            }
        } else if (line.find("a=fingerprint") != std::string::npos) {
            std::vector<std::string> items;
            rtc::tokenize(line, ' ', &items);
            if (items.size() != 2) {
                RTC_LOG(LS_WARNING) << "parse fingerprint error: " << line;
                return -1;
            }
            // a=fingerprint:14
            std::string alg = items[0].substr(14);
            absl::c_transform(alg, alg.begin(), ::tolower);
            std::string content = items[1];
            if (is_rn) {
                content = content.substr(0, content.length() - 1);
            }
            td->identity_fingerprint = rtc::SSLFingerprint::CreateUniqueFromRfc4572(
                    alg, content
            );
            if (!(td->identity_fingerprint.get())) {
                RTC_LOG(LS_WARNING) << "parse fingerprint error: " << line;
                return -1;
            }

        }
        return 0;
    }

    int PeerConnection::set_remote_sdp(const std::string &sdp) {
        std::vector<std::string> fields;
        size_t size = rtc::tokenize(sdp, '\n', &fields);
        if (size <= 0) {
            RTC_LOG(LS_WARNING) << "sdp invaild";
            return -1;
        }
        bool is_rn = false;
        if (sdp.find("\r\n") != std::string::npos) {
            is_rn = true;
        }
        std::string media_type;
        _remote_desc = std::make_unique<SessionDescription>(SdpType::k_answer);

        auto audio_content = std::make_shared<AudioContentDescription>();
        auto video_content = std::make_shared<VideoContentDescription>();
        auto audio_td = std::make_shared<TransportDescription>();
        auto video_td = std::make_shared<TransportDescription>();
        for (auto field: fields) {
            if(field.find("m=group:BUNDILE") != std::string::npos) {
                std::vector<std::string> items;
                if(is_rn)
                    field = field.substr(0, field.length() - 1);
                rtc::split(field, ' ', &items);
                if (items.size() <= 1) {
                    RTC_LOG(LS_WARNING) << "parse m=group error: " << field;
                    return -1;
                }
                ContentGroup answer_bundle("BUNDLE");
                for (size_t i = 1; i < items.size(); i++) {
                    answer_bundle.add_content_name(items[i]);
                }
                if (!answer_bundle.contents_names().empty()) {
                    _remote_desc->add_group(answer_bundle);
                }
            }
            if (field.find("m=") != std::string::npos) {
                std::vector<std::string> items;
                rtc::split(field, ' ', &items);
                if (items.size() <= 2) {
                    RTC_LOG(LS_WARNING) << "parse m= error: " << field;
                    return -1;
                }
                media_type = items[0].substr(2);
                if ("audio" == media_type) {
                    _remote_desc->add_content(audio_content);
                    audio_td->mid = "audio";
                } else if ("video" == media_type) {
                    _remote_desc->add_content(video_content);
                    video_td->mid = "video";
                }
            }
            if ("audio" == media_type) {
                if (parse_transport_info(audio_td.get(), field, is_rn) != 0) {
                    return -1;
                }
            } else if ("video" == media_type) {
                if (parse_transport_info(video_td.get(), field, is_rn) != 0) {
                    return -1;
                }
            }
        }
        _remote_desc->add_transport_info(audio_td);
        _remote_desc->add_transport_info(video_td);
        _transport_controller->set_remote_description(_remote_desc.get());

        return 0;

    }
    void destroy_timer_cb(EventLoop* /*el*/,TimerWatcher* /*w*/,void* data){
        PeerConnection* pc = (PeerConnection*)data;
        delete pc;
    }
    void PeerConnection::destroy() {
        if(_destroy_timer){
            _el->delete_timer(_destroy_timer);
            _destroy_timer = nullptr;
        }
        _destroy_timer = _el->create_timer(destroy_timer_cb,this,false);
        // 让pc的销毁跳出当前方法,避免在on_check中删除了
        _el->start_timer(_destroy_timer,10000); // 延时10ms销毁

    }
}
