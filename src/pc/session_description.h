

#ifndef  __SESSION_DESCRIPTION_H_
#define  __SESSION_DESCRIPTION_H_

#include <string>
#include <vector>
#include <memory>

#include <rtc_base/ssl_fingerprint.h>

#include "ice/ice_credentials.h"
#include "ice/candidate.h"
#include "pc/codec_info.h"
#include "pc/stream_params.h"

namespace xrtc {

enum class SdpType {
    k_offer = 0,
    k_answer = 1,
};

enum class MediaType {
    MEDIA_TYPE_AUDIO,
    MEDIA_TYPE_VIDEO
};

enum class RtpDirection {
    k_send_recv,
    k_send_only,
    k_recv_only,
    k_inactive
};

class MediaContentDescription {
public:
    virtual ~MediaContentDescription() {}
    virtual MediaType type() = 0;
    virtual std::string mid() = 0;
    
    const std::vector<std::shared_ptr<CodecInfo>>& get_codecs() const {
        return _codecs;
    }
    
    RtpDirection direction() { return _direction; }
    void set_direction(RtpDirection direction) { _direction = direction; }
    
    bool rtcp_mux() { return _rtcp_mux; }
    void set_rtcp_mux(bool mux) { _rtcp_mux = mux; }
   
    const std::vector<Candidate>& candidates() { return _candidates; }
    void add_candidates(const std::vector<Candidate>& candidates) {
        _candidates = candidates;
    }
    
    const std::vector<StreamParams>& streams() { return _send_streams; }
    void add_stream(const StreamParams& stream) {
        _send_streams.push_back(stream);
    }

protected:
    std::vector<std::shared_ptr<CodecInfo>> _codecs;
    RtpDirection _direction;
    bool _rtcp_mux = true;
    std::vector<Candidate> _candidates;
    std::vector<StreamParams> _send_streams;
};

class AudioContentDescription : public MediaContentDescription {
public:
    AudioContentDescription();
    MediaType type() override { return MediaType::MEDIA_TYPE_AUDIO; }
    std::string mid() override { return "audio"; }
};

class VideoContentDescription : public MediaContentDescription {
public:
    VideoContentDescription();
    MediaType type() override { return MediaType::MEDIA_TYPE_VIDEO; }
    std::string mid() override { return "video"; }
};

class ContentGroup {
public:
    ContentGroup(const std::string& semantics) : _semantics(semantics) {}
    ~ContentGroup() {}

    std::string semantics() const { return _semantics; }
    const std::vector<std::string>& content_names() const { return _content_names; }
    bool has_content_name(const std::string& content_name);
    void add_content_name(const std::string& content_name);

private:
    std::string _semantics;
    std::vector<std::string> _content_names;
};

enum ConnectionRole {
    NONE = 0,
    ACTIVE,
    PASSIVE,
    ACTPASS,
    HOLDCONN
};

class TransportDescription {
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
 
    std::shared_ptr<MediaContentDescription> get_content(const std::string& mid);
    void add_content(std::shared_ptr<MediaContentDescription> content);
    const std::vector<std::shared_ptr<MediaContentDescription>>& contents() const {
        return _contents;
    }
    
    void add_group(const ContentGroup& group);
    std::vector<const ContentGroup*> get_group_by_name(const std::string& name) const;
    
    bool add_transport_info(const std::string& mid, const IceParameters& ice_param,
            rtc::RTCCertificate* certificate);
    bool add_transport_info(std::shared_ptr<TransportDescription> td);
    std::shared_ptr<TransportDescription> get_transport_info(const std::string& mid);
    
    bool is_bundle(const std::string& mid);
    std::string get_first_bundle_mid();

    std::string to_string();

private:
    SdpType _sdp_type;
    std::vector<std::shared_ptr<MediaContentDescription>> _contents;
    std::vector<ContentGroup> _content_groups;
    std::vector<std::shared_ptr<TransportDescription>> _transport_infos;
};

} // namespace xrtc

#endif  //__SESSION_DESCRIPTION_H_


