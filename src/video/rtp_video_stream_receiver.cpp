#include "video/rtp_video_stream_receiver.h"

namespace xrtc {
namespace {

std::unique_ptr<RtpRtcpImpl> CreateRtpRtcpModule(
        const VideoReceiveStreamConfig& vconf,
        ReceiveStat* receive_stat) 
{
    RtpRtcpConfig config;
    config.el = vconf.el;
    config.clock = vconf.clock;
    config.local_media_ssrc = vconf.rtp.local_ssrc;
    config.receive_stat = receive_stat;
    config.rtp_rtcp_module_observer = vconf.rtp_rtcp_module_observer;

    auto rtp_rtcp = std::make_unique<RtpRtcpImpl>(config);
    rtp_rtcp->SetRTCPStatus(webrtc::RtcpMode::kCompound);
    return rtp_rtcp;
}

} // namespace 

RtpVideoStreamReceiver::RtpVideoStreamReceiver(const VideoReceiveStreamConfig& config,
        ReceiveStat* rtp_receive_stat) :
    config_(config),
    rtp_receive_stat_(rtp_receive_stat),
    rtp_rtcp_(CreateRtpRtcpModule(config, rtp_receive_stat))
{
    rtp_rtcp_->SetRemoteSsrc(config.rtp.remote_ssrc);
}

RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {

}

void RtpVideoStreamReceiver::OnRtpPacket(const webrtc::RtpPacketReceived& packet) {
    if (!packet.recovered())  {
        rtp_receive_stat_->OnRtpPacket(packet);
    }
}

void RtpVideoStreamReceiver::DeliverRtcp(const uint8_t* data, size_t len) {
    rtp_rtcp_->IncomingRtcpPacket(data, len);
}

} // namespace xrtc


