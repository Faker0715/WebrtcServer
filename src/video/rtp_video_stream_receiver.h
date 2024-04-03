//
// Created by faker on 24-1-30.
//

#ifndef XRTCSERVER_RTP_VIDEO_STREAM_RECEIVER_H
#define XRTCSERVER_RTP_VIDEO_STREAM_RECEIVER_H

#include "video_receive_stream_config.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "module/rtp_rtcp/receive_stat.h"
#include "module/rtp_rtcp/rtp_rtcp_impl.h"
#include <modules/rtp_rtcp/source/video_rtp_depacketizer_h264.h>
#include <modules/video_coding/packet_buffer.h>


namespace xrtc{
    class RtpVideoStreamReceiver{;
    public:
        RtpVideoStreamReceiver(const VideoReceiveStreamConfig& config,
                               ReceiveStat* rtp_receive_stat);
        ~RtpVideoStreamReceiver();
        void OnRtpPacket(const webrtc::RtpPacketReceived& packet);
        void DeliverRtcp(const uint8_t* data, size_t length);
    private:
        void ReceivePacket(const webrtc::RtpPacketReceived& packet);

        void OnReceivedPayloadData(
                rtc::CopyOnWriteBuffer codec_payload,
                const webrtc::RtpPacketReceived& packet,
                const webrtc::RTPVideoHeader& video_header);

        void OnInsertedPacket(webrtc::video_coding::PacketBuffer::InsertResult result);
    private:
        VideoReceiveStreamConfig config_;
        ReceiveStat* rtp_receive_stat_;
        std::unique_ptr<RtpRtcpImpl> rtp_rtcp_;
        std::unique_ptr<webrtc::VideoRtpDepacketizer> video_rtp_depacketizer_;
        std::unique_ptr<webrtc::video_coding::PacketBuffer> packet_buffer_;
    };
}


#endif //XRTCSERVER_RTP_VIDEO_STREAM_RECEIVER_H
