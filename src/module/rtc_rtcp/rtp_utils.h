//
// Created by faker on 23-5-1.
//

#ifndef XRTCSERVER_RTP_UTILS_H
#define XRTCSERVER_RTP_UTILS_H

#include "api/array_view.h"

namespace xrtc{
    enum class RtpPacketType{
        k_rtp,
        k_rtcp,
        k_unknown
    };
    RtpPacketType infer_rtp_packet_type(rtc::ArrayView<const char> packet);
    uint16_t parse_rtp_sequence_number(rtc::ArrayView<const uint8_t> packet);
    uint32_t parse_rtp_ssrc(rtc::ArrayView<const uint8_t> packet);
    bool get_rtcp_type(const void* data,int len,int* type);


}


#endif //XRTCSERVER_RTP_UTILS_H
