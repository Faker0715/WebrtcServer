#ifndef  __XRTCSERVER_PC_SESSION_DESCRIPTION_H_
#define  __XRTCSERVER_PC_SESSION_DESCRIPTION_H_

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
    kOffer = 0,
    kAnswer = 1,
};

enum class MediaType {
    MEDIA_TYPE_AUDIO,
    MEDIA_TYPE_VIDEO
};

enum class RtpDirection {
    kSendRecv,
    kSendOnly,
    kRecvOnly,
    kInactive
};

class MediaContentDescription {
public:
    virtual ~MediaContentDescription() {}
    virtual MediaType type() = 0;
    virtual std::string mid() = 0;
    
    const std::vector<std::shared_ptr<CodecInfo>>& codecs() const {
        return codecs_;
    }
    
    RtpDirection direction() { return direction_; }
    void set_direction(RtpDirection direction) { direction_ = direction; }
    
    bool rtcp_mux() { return rtcp_mux_; }
    void set_rtcp_mux(bool mux) { rtcp_mux_ = mux; }
   
    const std::vector<Candidate>& candidates() { return candidates_; }
    void add_candidates(const std::vector<Candidate>& candidates) {
        candidates_ = candidates;
    }
    
    const std::vector<StreamParams>& streams() { return send_streams_; }
    void add_stream(const StreamParams& stream) {
        send_streams_.push_back(stream);
    }

protected:
    std::vector<std::shared_ptr<CodecInfo>> codecs_;
    RtpDirection direction_;
    bool rtcp_mux_ = true;
    std::vector<Candidate> candidates_;
    std::vector<StreamParams> send_streams_;
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
    ContentGroup(const std::string& semantics) : semantics_(semantics) {}
    ~ContentGroup() {}

    std::string semantics() const { return semantics_; }
    const std::vector<std::string>& content_names() const { return content_names_; }
    bool HasContentName(const std::string& content_name);
    void AddContentName(const std::string& content_name);

private:
    std::string semantics_;
    std::vector<std::string> content_names_;
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
 
    std::shared_ptr<MediaContentDescription> GetContent(const std::string& mid);
    void AddContent(std::shared_ptr<MediaContentDescription> content);
    const std::vector<std::shared_ptr<MediaContentDescription>>& contents() const {
        return contents_;
    }
    
    void AddGroup(const ContentGroup& group);
    std::vector<const ContentGroup*> GetGroupByName(const std::string& name) const;
    
    bool AddTransportInfo(const std::string& mid, const IceParameters& ice_param,
            rtc::RTCCertificate* certificate);
    bool AddTransportInfo(std::shared_ptr<TransportDescription> td);
    std::shared_ptr<TransportDescription> GetTransportInfo(const std::string& mid);
    
    bool IsBundle(const std::string& mid);
    std::string GetFirstBundleMid();

    std::string ToString();

private:
    SdpType sdp_type_;
    std::vector<std::shared_ptr<MediaContentDescription>> contents_;
    std::vector<ContentGroup> content_groups_;
    std::vector<std::shared_ptr<TransportDescription>> transport_infos_;
};

} // namespace xrtc

#endif  //__XRTCSERVER_PC_SESSION_DESCRIPTION_H_


