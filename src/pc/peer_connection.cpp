#include <absl/algorithm/container.h>
#include <rtc_base/logging.h>

#include "ice/ice_credentials.h"
#include "pc/peer_connection.h"

namespace xrtc {

    struct SsrcInfo {
        uint32_t ssrc_id;
        std::string cname;
        std::string stream_id;
        std::string track_id;
    };

    static RtpDirection get_direction(bool send, bool recv) {
        if (send && recv) {
            return RtpDirection::k_send_recv;
        } else if (send && !recv) {
            return RtpDirection::k_send_only;
        } else if (!send && recv) {
            return RtpDirection::k_recv_only;
        } else {
            return RtpDirection::k_inactive;
        }
    }

    PeerConnection::PeerConnection(EventLoop *el, PortAllocator *allocator) :
            _el(el),
            _transport_controller(new TransportController(el, allocator)) {
        _transport_controller->signal_candidate_allocate_done.connect(this,
                                                                      &PeerConnection::_on_candidate_allocate_done);
        _transport_controller->signal_connection_state.connect(this,
                                                               &PeerConnection::_on_connection_state);
        _transport_controller->signal_rtp_packet_received.connect(this,
                                                                  &PeerConnection::_on_rtp_packet_received);
        _transport_controller->signal_rtcp_packet_received.connect(this,
                                                                   &PeerConnection::_on_rtcp_packet_received);
    }

    PeerConnection::~PeerConnection() {
        if (_destroy_timer) {
            _el->delete_timer(_destroy_timer);
            _destroy_timer = nullptr;
        }

        RTC_LOG(LS_INFO) << "PeerConnection destroy";
    }

    void PeerConnection::_on_candidate_allocate_done(TransportController * /*transport_controller*/,
                                                     const std::string &transport_name,
                                                     IceCandidateComponent /*component*/,
                                                     const std::vector<Candidate> &candidates) {
        for (auto c: candidates) {
            RTC_LOG(LS_INFO) << "candidate gathered, transport_name: " << transport_name
                             << ", " << c.to_string();
        }

        if (!_local_desc) {
            return;
        }

        auto content = _local_desc->get_content(transport_name);
        if (content) {
            content->add_candidates(candidates);
        }
    }

    void PeerConnection::_on_connection_state(TransportController *, PeerConnectionState state) {
        signal_connection_state(this, state);
    }

    void PeerConnection::_on_rtp_packet_received(TransportController *,
                                                 rtc::CopyOnWriteBuffer *packet, int64_t ts) {
        signal_rtp_packet_received(this, packet, ts);
    }

    void PeerConnection::_on_rtcp_packet_received(TransportController *,
                                                  rtc::CopyOnWriteBuffer *packet, int64_t ts) {
        signal_rtcp_packet_received(this, packet, ts);
    }

    int PeerConnection::init(rtc::RTCCertificate *certificate) {
        _certificate = certificate;
        _transport_controller->set_local_certificate(certificate);
        return 0;
    }

    void destroy_timer_cb(EventLoop * /*el*/, TimerWatcher * /*w*/, void *data) {
        PeerConnection *pc = (PeerConnection *) data;
        delete pc;
    }

    void PeerConnection::destroy() {
        if (_destroy_timer) {
            _el->delete_timer(_destroy_timer);
            _destroy_timer = nullptr;
        }

        _destroy_timer = _el->create_timer(destroy_timer_cb, this, false);
        _el->start_timer(_destroy_timer, 10000); // 10ms
    }

    std::string PeerConnection::create_offer(const RTCOfferAnswerOptions &options) {
        if (options.dtls_on && !_certificate) {
            RTC_LOG(LS_WARNING) << "certificate is null";
            return "";
        }

        _local_desc = std::make_unique<SessionDescription>(SdpType::k_offer);

        IceParameters ice_param = IceCredentials::create_random_ice_credentials();

        if (options.recv_audio || options.send_audio) {
            auto audio = std::make_shared<AudioContentDescription>();
            audio->set_direction(get_direction(options.send_audio, options.recv_audio));
            audio->set_rtcp_mux(options.use_rtcp_mux);
            _local_desc->add_content(audio);
            _local_desc->add_transport_info(audio->mid(), ice_param, _certificate);

            if (options.send_audio) {
                for (auto stream: _audio_source) {
                    audio->add_stream(stream);
                }
            }
        }

        if (options.recv_video || options.send_video) {
            auto video = std::make_shared<VideoContentDescription>();
            video->set_direction(get_direction(options.send_video, options.recv_video));
            video->set_rtcp_mux(options.use_rtcp_mux);
            _local_desc->add_content(video);
            _local_desc->add_transport_info(video->mid(), ice_param, _certificate);

            if (options.send_video) {
                for (auto stream: _video_source) {
                    video->add_stream(stream);
                }
            }
        }

        if (options.use_rtp_mux) {
            ContentGroup offer_bundle("BUNDLE");
            for (auto content: _local_desc->contents()) {
                offer_bundle.add_content_name(content->mid());
            }

            if (!offer_bundle.content_names().empty()) {
                _local_desc->add_group(offer_bundle);
            }
        }

        _transport_controller->set_local_description(_local_desc.get());

        return _local_desc->to_string();
    }

    static std::string get_attribute(const std::string &line) {
        std::vector<std::string> fields;
        size_t size = rtc::tokenize(line, ':', &fields);
        if (size != 2) {
            RTC_LOG(LS_WARNING) << "get attribute error: " << line;
            return "";
        }

        return fields[1];
    }

    static int parse_transport_info(TransportDescription *td, const std::string &line) {
        if (line.find("a=ice-ufrag") != std::string::npos) {
            td->ice_ufrag = get_attribute(line);
            if (td->ice_ufrag.empty()) {
                return -1;
            }
        } else if (line.find("a=ice-pwd") != std::string::npos) {
            td->ice_pwd = get_attribute(line);
            if (td->ice_pwd.empty()) {
                return -1;
            }
        } else if (line.find("a=fingerprint") != std::string::npos) {
            std::vector<std::string> items;
            rtc::tokenize(line, ' ', &items);
            if (items.size() != 2) {
                RTC_LOG(LS_WARNING) << "parse a=fingerprint error: " << line;
                return -1;
            }

            // a=fingerprint: 14
            std::string alg = items[0].substr(14);
            absl::c_transform(alg, alg.begin(), ::tolower);
            std::string content = items[1];

            td->identity_fingerprint = rtc::SSLFingerprint::CreateUniqueFromRfc4572(
                    alg, content);
            if (!(td->identity_fingerprint.get())) {
                RTC_LOG(LS_WARNING) << "create fingerprint error: " << line;
                return -1;
            }
        }

        return 0;
    }

    static int parse_ssrc_info(std::vector<SsrcInfo> &ssrc_info, const std::string &line) {
        if (line.find("a=ssrc:") == std::string::npos) {
            return 0;
        }

        //rfc5576
        // a=ssrc:<ssrc-id> <attribute>
        // a=ssrc:<ssrc-id> <attribute>:<value>
        std::string field1, field2;
        if (!rtc::tokenize_first(line.substr(2), ' ', &field1, &field2)) {
            RTC_LOG(LS_WARNING) << "parse a=ssrc failed, line: " << line;
            return -1;
        }

        // ssrc:<ssrc-id>
        std::string ssrc_id_s = field1.substr(5);
        uint32_t ssrc_id = 0;
        if (!rtc::FromString(ssrc_id_s, &ssrc_id)) {
            RTC_LOG(LS_WARNING) << "invalid ssrc_id, line: " << line;
            return -1;
        }

        // <attribute>
        std::string attribute;
        std::string value;
        if (!rtc::tokenize_first(field2, ':', &attribute, &value)) {
            RTC_LOG(LS_WARNING) << "get ssrc attribute failed, line: " << line;
            return -1;
        }

        auto iter = ssrc_info.begin();
        for (; iter != ssrc_info.end(); ++iter) {
            if (iter->ssrc_id == ssrc_id) {
                break;
            }
        }

        if (iter == ssrc_info.end()) {
            SsrcInfo info;
            info.ssrc_id = ssrc_id;
            ssrc_info.push_back(info);
            iter = ssrc_info.end() - 1;
        }

        if ("cname" == attribute) {
            iter->cname = value;
        } else if ("msid" == attribute) {
            std::vector<std::string> fields;
            rtc::split(value, ' ', &fields);
            if (fields.size() < 1 || fields.size() > 2) {
                RTC_LOG(LS_WARNING) << "msid format error, line: " << line;
                return -1;
            }

            iter->stream_id = fields[0];
            if (fields.size() == 2) {
                iter->track_id = fields[1];
            }
        }

        return 0;
    }

    static int parse_ssrc_group_info(std::vector<SsrcGroup> &ssrc_groups,
                                     const std::string &line) {
        if (line.find("a=ssrc-group:") == std::string::npos) {
            return 0;
        }

        // rfc5576
        // a=ssrc-group:<semantics> <ssrc-id> ...
        std::vector<std::string> fields;
        rtc::split(line.substr(2), ' ', &fields);
        if (fields.size() < 2) {
            RTC_LOG(LS_WARNING) << "ssrc-group field size < 2, line: " << line;
            return -1;
        }

        std::string semantics = get_attribute(fields[0]);
        if (semantics.empty()) {
            return -1;
        }

        std::vector<uint32_t> ssrcs;
        for (size_t i = 1; i < fields.size(); ++i) {
            uint32_t ssrc_id = 0;
            if (!rtc::FromString(fields[i], &ssrc_id)) {
                return -1;
            }
            ssrcs.push_back(ssrc_id);
        }

        ssrc_groups.push_back(SsrcGroup(semantics, ssrcs));

        return 0;
    }

    static void create_track_from_ssrc_info(const std::vector<SsrcInfo> &ssrc_infos,
                                            std::vector<StreamParams> &tracks) {
        for (auto ssrc_info: ssrc_infos) {
            std::string track_id = ssrc_info.track_id;

            auto iter = tracks.begin();
            for (; iter != tracks.end(); ++iter) {
                if (iter->id == track_id) {
                    break;
                }
            }

            if (iter == tracks.end()) {
                StreamParams track;
                track.id = track_id;
                tracks.push_back(track);
                iter = tracks.end() - 1;
            }

            iter->cname = ssrc_info.cname;
            iter->stream_id = ssrc_info.stream_id;
            iter->ssrcs.push_back(ssrc_info.ssrc_id);
        }
    }

    int PeerConnection::set_remote_sdp(const std::string &sdp) {
        std::vector<std::string> fields;
        size_t size = rtc::tokenize(sdp, '\n', &fields);
        if (size <= 0) {
            RTC_LOG(LS_WARNING) << "sdp invalid";
            return -1;
        }

        bool is_rn = false;
        if (sdp.find("\r\n") != std::string::npos) {
            is_rn = true;
        }

        _remote_desc = std::make_unique<SessionDescription>(SdpType::k_answer);

        std::string media_type;

        auto audio_content = std::make_shared<AudioContentDescription>();
        auto video_content = std::make_shared<VideoContentDescription>();
        auto audio_td = std::make_shared<TransportDescription>();
        auto video_td = std::make_shared<TransportDescription>();
        std::vector<SsrcInfo> audio_ssrc_info;
        std::vector<SsrcInfo> video_ssrc_info;
        std::vector<SsrcGroup> video_ssrc_groups;
        std::vector<StreamParams> audio_tracks;
        std::vector<StreamParams> video_tracks;

        for (auto field: fields) {
            if (is_rn) {
                field = field.substr(0, field.length() - 1);
            }

            if (field.find("m=group:BUNDLE") != std::string::npos) {
                std::vector<std::string> items;
                rtc::split(field, ' ', &items);
                if (items.size() > 1) {
                    ContentGroup answer_bundle("BUNDLE");
                    for (size_t i = 1; i < items.size(); ++i) {
                        answer_bundle.add_content_name(items[i]);
                    }
                    _remote_desc->add_group(answer_bundle);
                }
            } else if (field.find("m=") != std::string::npos) {
                std::vector<std::string> items;
                rtc::split(field, ' ', &items);
                if (items.size() <= 2) {
                    RTC_LOG(LS_WARNING) << "parse m= error: " << field;
                    return -1;
                }

                // m=audio/video
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
                if (parse_transport_info(audio_td.get(), field) != 0) {
                    return -1;
                }

                if (parse_ssrc_info(audio_ssrc_info, field) != 0) {
                    return -1;
                }

            } else if ("video" == media_type) {
                if (parse_transport_info(video_td.get(), field) != 0) {
                    return -1;
                }

                if (parse_ssrc_group_info(video_ssrc_groups, field) != 0) {
                    return -1;
                }

                if (parse_ssrc_info(video_ssrc_info, field) != 0) {
                    return -1;
                }
            }
        }

        if (!audio_ssrc_info.empty()) {
            create_track_from_ssrc_info(audio_ssrc_info, audio_tracks);

            for (auto track: audio_tracks) {
                audio_content->add_stream(track);
            }
        }

        if (!video_ssrc_info.empty()) {
            create_track_from_ssrc_info(video_ssrc_info, video_tracks);

            for (auto ssrc_group: video_ssrc_groups) {
                if (ssrc_group.ssrcs.empty()) {
                    continue;
                }

                uint32_t ssrc = ssrc_group.ssrcs.front();
                for (StreamParams &track: video_tracks) {
                    if (track.has_ssrc(ssrc)) {
                        track.ssrc_groups.push_back(ssrc_group);
                    }
                }
            }

            for (auto track: video_tracks) {
                video_content->add_stream(track);
            }
        }

        _remote_desc->add_transport_info(audio_td);
        _remote_desc->add_transport_info(video_td);

        _transport_controller->set_remote_description(_remote_desc.get());
        return 0;
    }

    int PeerConnection::send_rtp(const char *data, size_t len) {
        if (_transport_controller) {
            // todo: 需要根据实际情况完善
            return _transport_controller->send_rtp("audio", data, len);
        }

        return -1;
    }

    int PeerConnection::send_rtcp(const char *data, size_t len) {
        if (_transport_controller) {
            // todo: 需要根据实际情况完善
            return _transport_controller->send_rtcp("audio", data, len);
        }

        return -1;
    }

} // namespace xrtc


