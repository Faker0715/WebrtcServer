//
// Created by faker on 24-1-30.
//

#ifndef XRTCSERVER_VIDEO_RECEIVE_STREAM_H
#define XRTCSERVER_VIDEO_RECEIVE_STREAM_H
#include "video_receive_stream_config.h"
#include "rtp_video_stream_receiver.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "module/rtp_rtcp/receive_stat.h"

namespace xrtc {

    class VideoReceiveStream{
    public:
        VideoReceiveStream(const VideoReceiveStreamConfig& config);
        ~VideoReceiveStream();
        void OnRtpPacket(const webrtc::RtpPacketReceived& packet);

    private:
        VideoReceiveStreamConfig config_;
        std::unique_ptr<ReceiveStat> rtp_receive_stat_;
        RtpVideoStreamReceiver rtp_video_stream_receiver_;
    };
}


#endif //XRTCSERVER_VIDEO_RECEIVE_STREAM_H
