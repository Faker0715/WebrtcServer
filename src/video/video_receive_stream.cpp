#include "video/video_receive_stream.h"

namespace xrtc {

VideoReceiveStream::VideoReceiveStream(const VideoReceiveStreamConfig& config) :
    config_(config),
    rtp_receive_stat_(ReceiveStat::Create(config.clock)),
    rtp_video_stream_receiver_(config, rtp_receive_stat_.get())
{
}

VideoReceiveStream::~VideoReceiveStream() {
}

void VideoReceiveStream::OnRtpPacket(const webrtc::RtpPacketReceived& packet) {
    rtp_video_stream_receiver_.OnRtpPacket(packet);
}

void VideoReceiveStream::DeliverRtcp(const uint8_t* data, size_t len) {
    rtp_video_stream_receiver_.DeliverRtcp(data, len);
}

} // namespace xrtc


