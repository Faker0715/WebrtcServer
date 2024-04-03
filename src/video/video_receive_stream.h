#ifndef  XRTCSERVER_VIDEO_VIDEO_RECEIVE_STREAM_H_
#define  XRTCSERVER_VIDEO_VIDEO_RECEIVE_STREAM_H_

#include "video/video_receive_stream_config.h"
#include "video/rtp_video_stream_receiver.h"

namespace xrtc {

class VideoReceiveStream {
public:
    VideoReceiveStream(const VideoReceiveStreamConfig& config);
    ~VideoReceiveStream();
    
    void OnRtpPacket(const webrtc::RtpPacketReceived& packet);
    void DeliverRtcp(const uint8_t* data, size_t len);

private:
    VideoReceiveStreamConfig config_;
    std::unique_ptr<ReceiveStat> rtp_receive_stat_;
    RtpVideoStreamReceiver rtp_video_stream_receiver_;
};

} // namespace xrtc

#endif  //XRTCSERVER_VIDEO_VIDEO_RECEIVE_STREAM_H_


