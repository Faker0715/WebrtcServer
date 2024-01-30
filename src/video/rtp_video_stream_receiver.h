//
// Created by faker on 24-1-30.
//

#ifndef XRTCSERVER_RTP_VIDEO_STREAM_RECEIVER_H
#define XRTCSERVER_RTP_VIDEO_STREAM_RECEIVER_H

#include "video_receive_stream_config.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace xrtc{
    class RtpVideoStreamReceiver{;
    public:
        RtpVideoStreamReceiver(const VideoReceiveStreamConfig& config);
        ~RtpVideoStreamReceiver();
        void OnRtpPacket(const webrtc::RtpPacketReceived& packet);
    private:
        VideoReceiveStreamConfig config_;
    };
}


#endif //XRTCSERVER_RTP_VIDEO_STREAM_RECEIVER_H
