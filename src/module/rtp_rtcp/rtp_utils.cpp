//
// Created by faker on 23-5-1.
//

#include <rtc_base/byte_io.h>
#include "module/rtp_rtcp/rtp_utils.h"

namespace xrtc {

    const uint8_t k_rtp_version = 2;
    const size_t k_min_rtp_packet_len = 12;
    const size_t k_min_rtcp_packet_len = 4;

    bool has_correct_rtp_version(rtc::ArrayView<const uint8_t> packet) {
        return packet[0] >> 6 == k_rtp_version;
    }

    bool payload_type_is_reserved_for_rtcp(uint8_t payload_type) {
        return 64 <= payload_type && payload_type < 96;
    }

    bool is_rtp_packet(rtc::ArrayView<const uint8_t> packet) {
        return packet.size() >= k_min_rtp_packet_len &&
               has_correct_rtp_version(packet) &&
               !payload_type_is_reserved_for_rtcp(packet[1] & 0x7F);
    }

    bool is_rtcp_packet(rtc::ArrayView<const uint8_t> packet) {
        return packet.size() >= k_min_rtcp_packet_len &&
               has_correct_rtp_version(packet) &&
               payload_type_is_reserved_for_rtcp(packet[1] & 0x7F);
    }

    RtpPacketType infer_rtp_packet_type(rtc::ArrayView<const char> packet) {
        if (is_rtp_packet(rtc::reinterpret_array_view<const uint8_t>(packet))) {
            return RtpPacketType::k_rtp;
        }

        if (is_rtcp_packet(rtc::reinterpret_array_view<const uint8_t>(packet))) {
            return RtpPacketType::k_rtcp;
        }

        return RtpPacketType::k_unknown;
    }

    uint16_t parse_rtp_sequence_number(rtc::ArrayView<const uint8_t> packet) {
        return rtc::ByteReader<uint16_t>::ReadBigEndian(packet.data() + 2);
    }

    uint32_t parse_rtp_ssrc(rtc::ArrayView<const uint8_t> packet) {
        return rtc::ByteReader<uint32_t>::ReadBigEndian(packet.data() + 8);
    }

    bool get_rtcp_type(const void* data, size_t len, int* type) {
        if (len < k_min_rtcp_packet_len) {
            return false;
        }

        if (!data || !type) {
            return false;
        }

        *type = *((const uint8_t*)data + 1);
        return true;
    }

} // namespace xrtc


