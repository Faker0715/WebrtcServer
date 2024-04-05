#ifndef  XRTCSERVER_VIDEO_RTP_VIDEO_STREAM_RECEIVER_H_
#define  XRTCSERVER_VIDEO_RTP_VIDEO_STREAM_RECEIVER_H_

#include <modules/rtp_rtcp/source/rtp_packet_received.h>
#include <modules/rtp_rtcp/source/video_rtp_depacketizer_h264.h>
#include <modules/video_coding/packet_buffer.h>
#include <modules/video_coding/nack_requester.h>

#include "video/video_receive_stream_config.h"
#include "modules/rtp_rtcp/receive_stat.h"
#include "modules/rtp_rtcp/rtp_rtcp_impl.h"
#include "modules/video_coding/rtp_frame_object.h"

namespace xrtc {

class RtpVideoStreamReceiver : public sigslot::has_slots<> {
public:
    RtpVideoStreamReceiver(const VideoReceiveStreamConfig& config,
            ReceiveStat* rtp_receive_stat);
    ~RtpVideoStreamReceiver();

    void OnRtpPacket(const webrtc::RtpPacketReceived& packet);
    void DeliverRtcp(const uint8_t* data, size_t len);

private:
    void ReceivePacket(const webrtc::RtpPacketReceived& packet);
    void OnReceivedPayloadData(
            rtc::CopyOnWriteBuffer codec_payload,
            const webrtc::RtpPacketReceived& packet,
            const webrtc::RTPVideoHeader& video_header);
    void OnInsertedPacket(webrtc::video_coding::PacketBuffer::InsertResult result);
    void OnAssembledFrame(std::unique_ptr<RtpFrameObject> frame);

    void OnNackSend(const std::vector<uint16_t>& nack_list);

private:
    VideoReceiveStreamConfig config_;
    ReceiveStat* rtp_receive_stat_;
    std::unique_ptr<RtpRtcpImpl> rtp_rtcp_;
    std::unique_ptr<webrtc::VideoRtpDepacketizer> video_rtp_depacketizer_;
    std::unique_ptr<webrtc::video_coding::PacketBuffer> packet_buffer_;
    std::unique_ptr<NackRequester> nack_module_;
};

} // namespace xrtc

#endif  //XRTCSERVER_VIDEO_RTP_VIDEO_STREAM_RECEIVER_H_