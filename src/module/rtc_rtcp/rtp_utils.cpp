//
// Created by faker on 23-5-1.
//

#include "rtp_utils.h"
namespace xrtc{
    const uint8_t k_rtp_version = 2;
    const size_t k_min_rtp_packet = 12;
    const size_t k_min_rtcp_packet = 4;

    bool has_correct_rtp_version(rtc::ArrayView<const uint8_t> packet){
        return packet[0] >> 6 == k_rtp_version;
    }
    bool payload_type_is_reserved_for_rtcp(uint8_t payload_type){
        return payload_type >= 64 && payload_type < 96;
    }
    bool is_rtp_packet(rtc::ArrayView<const uint8_t> packet) {
        return packet.size() >= k_min_rtp_packet &&
            has_correct_rtp_version(packet) &&
            !payload_type_is_reserved_for_rtcp(packet[1] & 0x7F);
    }

    bool is_rtcp_packet(rtc::ArrayView<const uint8_t> packet) {
        return packet.size() >= k_min_rtcp_packet &&
               has_correct_rtp_version(packet) &&
               payload_type_is_reserved_for_rtcp(packet[1] & 0x7F);
    }

    RtpPacketType infer_rtp_packet_type(rtc::ArrayView<const char> packet) {
        if(is_rtp_packet(rtc::reinterpret_array_view<const uint8_t>(packet))){
            return xrtc::RtpPacketType::k_rtp;
        }
        if(is_rtcp_packet(rtc::reinterpret_array_view<const uint8_t>(packet))){
            return xrtc::RtpPacketType::k_rtcp;
        }
        return xrtc::RtpPacketType::k_unknown;
    }
}
