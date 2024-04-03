#include "pc/session_description.h"

#include <sstream>

#include <rtc_base/logging.h>

namespace xrtc {

const char kMediaProtocolDtlsSavpf[] = "UDP/TLS/RTP/SAVPF";
const char kMediaProtocolSavpf[] = "RTP/SAVPF";

AudioContentDescription::AudioContentDescription() {
    auto codec = std::make_shared<AudioCodecInfo>();
    codec->id = 111;
    codec->name = "opus";
    codec->clockrate = 48000;
    codec->channels = 2;
    
    // add feedback param
    codec->feedback_param.push_back(FeedbackParam("transport-cc"));
    
    // add codec param
    codec->codec_param["minptime"] = "10";
    codec->codec_param["useinbandfec"] = "1";

    codecs_.push_back(codec);
}

VideoContentDescription::VideoContentDescription() {
    auto codec = std::make_shared<VideoCodecInfo>();
    codec->id = 107;
    codec->name = "H264";
    codec->clockrate = 90000;
    codecs_.push_back(codec);

    // add feedback param
    codec->feedback_param.push_back(FeedbackParam("goog-remb"));
    codec->feedback_param.push_back(FeedbackParam("transport-cc"));
    codec->feedback_param.push_back(FeedbackParam("ccm", "fir"));
    codec->feedback_param.push_back(FeedbackParam("nack"));
    codec->feedback_param.push_back(FeedbackParam("nack", "pli"));

    // add codec param
    codec->codec_param["level-asymmetry-allowed"] = "1";
    codec->codec_param["packetization-mode"] = "1";
    codec->codec_param["profile-level-id"] = "42e01f";
     
    auto rtx_codec = std::make_shared<VideoCodecInfo>();
    rtx_codec->id = 99;
    rtx_codec->name = "rtx";
    rtx_codec->clockrate = 90000;
    
    // add codec param
    rtx_codec->codec_param["apt"] = std::to_string(codec->id);
    codecs_.push_back(rtx_codec);
}

bool ContentGroup::HasContentName(const std::string& content_name) {
    for (auto name : content_names_) {
        if (name == content_name) {
            return true;
        }
    }

    return false;
}

void ContentGroup::AddContentName(const std::string& content_name) {
    if (!HasContentName(content_name)) {
        content_names_.push_back(content_name);
    }
}

SessionDescription::SessionDescription(SdpType type) :
    sdp_type_(type)
{
}

SessionDescription::~SessionDescription() {

}

std::shared_ptr<MediaContentDescription> SessionDescription::GetContent(
        const std::string& mid)
{
    for (auto content : contents_) {
        if (mid == content->mid()) {
            return content;
        }
    }

    return nullptr;
}

void SessionDescription::AddContent(std::shared_ptr<MediaContentDescription> content) {
    contents_.push_back(content);
}

void SessionDescription::AddGroup(const ContentGroup& group) {
    content_groups_.push_back(group);
}

std::vector<const ContentGroup*> SessionDescription::GetGroupByName(
        const std::string& name) const
{
    std::vector<const ContentGroup*> content_groups;
    for (const ContentGroup& group : content_groups_) {
        if (group.semantics() == name) {
            content_groups.push_back(&group);
        }
    }

    return content_groups;
}

static std::string ConnectionRoleToString(ConnectionRole role) {
    switch (role) {
        case ConnectionRole::ACTIVE:
            return "active";
        case ConnectionRole::PASSIVE:
            return "passive";
        case ConnectionRole::ACTPASS:
            return "actpass";
        case ConnectionRole::HOLDCONN:
            return "holdconn";
        default:
            return "none";
    }
}

bool SessionDescription::AddTransportInfo(const std::string& mid, 
        const IceParameters& ice_param,
        rtc::RTCCertificate* certificate)
{
    auto tdesc = std::make_shared<TransportDescription>();
    tdesc->mid = mid;
    tdesc->ice_ufrag = ice_param.ice_ufrag;
    tdesc->ice_pwd = ice_param.ice_pwd;
    if (certificate) {
        tdesc->identity_fingerprint = rtc::SSLFingerprint::CreateFromCertificate(
                *certificate);
        if (!tdesc->identity_fingerprint) {
            RTC_LOG(LS_WARNING) << "get fingerprint failed";
            return false;
        }
    }
    
    if (SdpType::kOffer == sdp_type_) {
        tdesc->connection_role = ConnectionRole::ACTPASS;
    } else {
        tdesc->connection_role = ConnectionRole::ACTIVE;
    }

    transport_infos_.push_back(tdesc);

    return true;
}

bool SessionDescription::AddTransportInfo(std::shared_ptr<TransportDescription> td) {
    transport_infos_.push_back(td);
    return true;
}

std::shared_ptr<TransportDescription> SessionDescription::GetTransportInfo(
        const std::string& mid) 
{
    for (auto tdesc : transport_infos_) {
        if (tdesc->mid == mid) {
            return tdesc;
        }
    }

    return nullptr;
}

bool SessionDescription::IsBundle(const std::string& mid) {
    auto content_group = GetGroupByName("BUNDLE");
    if (content_group.empty()) {
        return false;
    }

    for (auto group : content_group) {
        for (auto name : group->content_names()) {
            if (name == mid) {
                return true;
            }
        }
    }

    return false;
}
    

std::string SessionDescription::GetFirstBundleMid() {
    auto content_group = GetGroupByName("BUNDLE");
    if (content_group.empty()) {
        return "";
    }
    
    return content_group[0]->content_names()[0];
}

static void AddRtcpFbLine(std::shared_ptr<CodecInfo> codec,
        std::stringstream& ss)
{
    for (auto param : codec->feedback_param) {
        ss << "a=rtcp-fb:" << codec->id << " " << param.id();
        if (!param.param().empty()) {
            ss << " " << param.param();
        }
        ss << "\r\n";
    }
}

static void AddFmtpLine(std::shared_ptr<CodecInfo> codec,
        std::stringstream& ss)
{
    if (!codec->codec_param.empty()) {
        ss << "a=fmtp:" << codec->id << " ";
        std::string data;
        for (auto param : codec->codec_param) {
            data += (";" + param.first + "=" + param.second);
        }
        // data = ";key1=value1;key2=value2"
        data = data.substr(1);
        ss << data << "\r\n";
    }
}

static void BuildRtpMap(std::shared_ptr<MediaContentDescription> content,
        std::stringstream& ss)
{
    for (auto codec : content->codecs()) {
        ss << "a=rtpmap:" << codec->id << " " << codec->name << "/" << codec->clockrate;
        if (MediaType::MEDIA_TYPE_AUDIO == content->type()) {
            auto audio_codec = codec->as_audio();
            ss << "/" << audio_codec->channels;
        }
        ss << "\r\n";

        AddRtcpFbLine(codec, ss);
        AddFmtpLine(codec, ss);
    }
}

static void BuildRtpDirection(std::shared_ptr<MediaContentDescription> content,
        std::stringstream& ss)
{
    switch (content->direction()) {
        case RtpDirection::kSendRecv:
            ss << "a=sendrecv\r\n";
            break;
        case RtpDirection::kSendOnly:
            ss << "a=sendonly\r\n";
            break;
        case RtpDirection::kRecvOnly:
            ss << "a=recvonly\r\n";
            break;
        default:
            ss << "a=inactive\r\n";
            break;
    }
}

static void BuildCandidates(std::shared_ptr<MediaContentDescription> content,
        std::stringstream& ss)
{
    for (auto c : content->candidates()) {
        ss << "a=candidate:" << c.foundation
           << " " << c.component
           << " " << c.protocol
           << " " << c.priority
           << " " << c.address.HostAsURIString()
           << " " << c.port
           << " typ " << c.type
           << "\r\n";
    }
}

static void AddSsrcLine(uint32_t ssrc,
        const std::string& attribute,
        const std::string& value,
        std::stringstream& ss)
{
    ss << "a=ssrc:" << ssrc << " " << attribute << ":" << value << "\r\n";
}

static void BuildSsrc(std::shared_ptr<MediaContentDescription> content,
        std::stringstream& ss)
{
    for (auto track : content->streams()) {
        for (auto ssrc_group : track.ssrc_groups) {
            if (ssrc_group.ssrcs.empty()) {
                continue;
            }

            ss << "a=ssrc-group:" << ssrc_group.semantics;
            for (auto ssrc : ssrc_group.ssrcs) {
                ss << " " << ssrc;
            }
            ss << "\r\n";
        }
    
        std::string msid = track.stream_id + " " + track.id;
        for (auto ssrc : track.ssrcs) {
            AddSsrcLine(ssrc, "cname", track.cname, ss);
            AddSsrcLine(ssrc, "msid", msid, ss);
            AddSsrcLine(ssrc, "mslabel", track.stream_id, ss);
            AddSsrcLine(ssrc, "lable", track.id, ss);
        }
    }
}

std::string SessionDescription::ToString() {
    std::stringstream ss;
    // version
    ss << "v=0\r\n";
	// session origin
	// RFC 4566
	// o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
	ss << "o=- 0 2 IN IP4 127.0.0.1\r\n";
	// session name
	ss << "s=-\r\n";
	// time description
	ss << "t=0 0\r\n";
  
    // BUNDLE
    std::vector<const ContentGroup*> content_group = GetGroupByName("BUNDLE");
    if (!content_group.empty()) {
        ss << "a=group:BUNDLE";
        for (auto group : content_group) {
            for (auto content_name : group->content_names()) {
                ss << " " << content_name;
            }
        }
        ss << "\r\n";
    }
    
    ss << "a=msid-semantic: WMS\r\n";

    for (auto content : contents_) {
        // RFC 4566
        // m=<media> <port> <proto> <fmt>
        std::string fmt;
        for (auto codec : content->codecs()) {
            fmt.append(" ");
            fmt.append(std::to_string(codec->id));
        }

        auto transport_info = GetTransportInfo(content->mid());
        if (transport_info && transport_info->identity_fingerprint.get()) {
            ss << "m=" << content->mid() << " 9 " << kMediaProtocolDtlsSavpf
                << fmt << "\r\n";
        } else {
            ss << "m=" << content->mid() << " 9 " << kMediaProtocolSavpf
                << fmt << "\r\n";
        }

        ss << "c=IN IP4 0.0.0.0\r\n";
        ss << "a=rtcp:9 IN IP4 0.0.0.0\r\n";
        
        BuildCandidates(content, ss);

        if (transport_info) {
            ss << "a=ice-ufrag:" << transport_info->ice_ufrag << "\r\n";
            ss << "a=ice-pwd:" << transport_info->ice_pwd << "\r\n";

            auto fp = transport_info->identity_fingerprint.get();
            if (fp) {
                ss << "a=fingerprint:" << fp->algorithm << " " << fp->GetRfc4572Fingerprint()
                    << "\r\n";
                ss << "a=setup:" << ConnectionRoleToString(
                        transport_info->connection_role) << "\r\n";
            }
        }

        ss << "a=mid:" << content->mid() << "\r\n";
        BuildRtpDirection(content, ss);

        if (content->rtcp_mux()) {
            ss << "a=rtcp-mux\r\n";
        }

        BuildRtpMap(content, ss);
        BuildSsrc(content, ss);
    }

    return ss.str();
}

} // namespace xrtc




