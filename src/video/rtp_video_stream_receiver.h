#ifndef  XRTCSERVER_VIDEO_RTP_VIDEO_STREAM_RECEIVER_H_
#define  XRTCSERVER_VIDEO_RTP_VIDEO_STREAM_RECEIVER_H_

#include <modules/rtp_rtcp/source/rtp_packet_received.h>

#include "video/video_receive_stream_config.h"
#include "modules/rtp_rtcp/receive_stat.h"
#include "modules/rtp_rtcp/rtp_rtcp_impl.h"

namespace xrtc {

class RtpVideoStreamReceiver {
public:
    RtpVideoStreamReceiver(const VideoReceiveStreamConfig& config,
            ReceiveStat* rtp_receive_stat);
    ~RtpVideoStreamReceiver();

    void OnRtpPacket(const webrtc::RtpPacketReceived& packet);
    void DeliverRtcp(const uint8_t* data, size_t len);

private:
    VideoReceiveStreamConfig config_;
    ReceiveStat* rtp_receive_stat_;
    std::unique_ptr<RtpRtcpImpl> rtp_rtcp_;
};

} // namespace xrtc

#endif  //XRTCSERVER_VIDEO_RTP_VIDEO_STREAM_RECEIVER_H_


