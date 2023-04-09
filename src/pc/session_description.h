//
// Created by faker on 23-4-2.
//

#ifndef XRTCSERVER_SESSION_DESCRIPTION_H
#define XRTCSERVER_SESSION_DESCRIPTION_H

#include <string>
#include <memory>
#include <vector>
#include "codec_info.h"
#include "ice/ice_credentials.h"
#include "ice/candidate.h"
#include <rtc_base/ssl_fingerprint.h>
#include <rtc_base/ssl_identity.h>
#include <rtc_base/rtc_certificate.h>

namespace xrtc {
    enum ConnectionRole{
        NONE = 0,
        ACTIVE = 1,
        PASSIVE = 2,
        ACTPASS = 3,
        HOLDCONN = 4,
    };
    enum class SdpType {
        k_offer = 0,
        k_answer = 1,
    };
    enum class MediaType {
        MEDIA_TYPE_AUDIO = 0,
        MEDIA_TYPE_VIDEO = 1,
    };

    enum class RtpDirection{
        k_send_recv = 0,
        k_send_only = 1,
        k_recv_only = 2,
        k_inactive = 3,
    };
    class MediaContentDescription {
    public:
        virtual ~MediaContentDescription(){};

        virtual MediaType type() = 0;

        virtual std::string mid() = 0;

        const std::vector<std::shared_ptr<CodecInfo>>& get_codecs() const {
            return _codecs;
        }
        RtpDirection direction() const {
            return _direction;
        }
        void set_direction(RtpDirection direction) {
            _direction = direction;
        }
        bool rtcp_mux() const {
            return _rtcp_mux;
        }
        void set_rtcp_mux(bool mux){
            _rtcp_mux = mux;
        }
        void add_candidates(const std::vector<Candidate>& candidates){
            _candidates = candidates;
        }
        const std::vector<Candidate>& candidates() const {
            return _candidates;
        }
    protected:
        std::vector<std::shared_ptr<CodecInfo>> _codecs;
        RtpDirection _direction;
        bool _rtcp_mux = true;
        std::vector<Candidate> _candidates;
    };

    class AudioContentDescription : public MediaContentDescription {
    public:
        AudioContentDescription();
        MediaType type() override {
            return MediaType::MEDIA_TYPE_AUDIO;
        }

        std::string mid() override {
            return "audio";
        }
    };

    class VideoContentDescription : public MediaContentDescription {
    public:
        VideoContentDescription();
        MediaType type() override {
            return MediaType::MEDIA_TYPE_VIDEO;
        }

        std::string mid() override {
            return "video";
        }
    };

    class ContentGroup {

    public:
        ContentGroup(const std::string semantics):_semantics(semantics){

        };
        ~ContentGroup(){

        };
        std::string semantics() const{
            return _semantics;
        }
        const std::vector<std::string>& contents_names() const{
            return _contents;
        }
        void add_content_name(const std::string& content_name);
        bool has_content_name(const std::string &content_name);
    private:
        std::string _semantics;
        std::vector<std::string> _contents;

    };
    class TransportDescription{
    public:
        std::string mid;
        std::string ice_ufrag;
        std::string ice_pwd;
        std::unique_ptr<rtc::SSLFingerprint> identity_fingerprint;
        ConnectionRole connection_role = ConnectionRole::NONE;
    };
    class SessionDescription {
    public:
        SessionDescription(SdpType type);

        ~SessionDescription();

        std::string to_string();

        bool is_bundle(const std::string& mid);
        std::string get_first_bundle_mid();
        std::shared_ptr<MediaContentDescription> get_content(const std::string& mid);
        void add_content(std::shared_ptr<MediaContentDescription> content);
        void add_group(const ContentGroup& group);
        bool add_transport_info(const std::string& mid,const IceParameters& ice_param,
                                rtc::RTCCertificate* certificate);
        std::vector<const ContentGroup*>  _get_group_by_name(const std::string& name) const;
        std::shared_ptr<TransportDescription> get_transport_info(const std::string &mid);
        const std::vector<std::shared_ptr<MediaContentDescription>>& contents() const{
            return _contents;
        }
    private:
        std::vector<std::shared_ptr<MediaContentDescription>> _contents;
        SdpType _sdp_type;
        std::vector<ContentGroup> _content_groups;
        std::vector<std::shared_ptr<TransportDescription>> _transport_infos;

    };
}


#endif //XRTCSERVER_SESSION_DESCRIPTION_H
