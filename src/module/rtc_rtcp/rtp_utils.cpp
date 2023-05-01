//
// Created by faker on 23-5-1.
//

#include "rtp_utils.h"
namespace xrtc{
    RtpPacketType infer_rtp_packet_type(rtc::ArrayView<const char> packet) {
        return xrtc::RtpPacketType::k_unknown;
    }
}
