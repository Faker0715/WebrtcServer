#ifndef  __XRTCSERVER_MODULES_VIDEO_CODING_RTP_FRAME_OBJECT_H_
#define  __XRTCSERVER_MODULES_VIDEO_CODING_RTP_FRAME_OBJECT_H_

#include <modules/rtp_rtcp/source/rtp_video_header.h>

namespace xrtc {

class RtpFrameObject {
public:
    RtpFrameObject(uint16_t first_seq_num,
            uint16_t last_seq_num,
            webrtc::VideoCodecType codec_type,
            const webrtc::RTPVideoHeader& video_header);
    ~RtpFrameObject();

    uint16_t first_seq_num() const { return first_seq_num_; }
    uint16_t last_seq_num() const { return last_seq_num_; }
    webrtc::VideoCodecType codec_type() const { return codec_type_; }
    webrtc::VideoFrameType frame_type() const {
        return video_header_.frame_type;
    }

private:
    uint16_t first_seq_num_;
    uint16_t last_seq_num_;
    webrtc::VideoCodecType codec_type_;
    webrtc::RTPVideoHeader video_header_;
};

} // namespace xrtc

#endif  //__XRTCSERVER_MODULES_VIDEO_CODING_RTP_FRAME_OBJECT_H_


