#include "modules/rtp_rtcp/rtp_utils.h"

#include <rtc_base/byte_io.h>

namespace xrtc {

const uint8_t kRtpVersion = 2;
const size_t kMinRtpPacketLen = 12;
const size_t kMinRtcpPacketLen = 4;

bool HasCorrectRtpVersion(rtc::ArrayView<const uint8_t> packet) {
    return packet[0] >> 6 == kRtpVersion;
}

bool PayloadTypeIsReservedForRtcp(uint8_t payload_type) {
    return 64 <= payload_type && payload_type < 96;
}

bool IsRtpPacket(rtc::ArrayView<const uint8_t> packet) {
    return packet.size() >= kMinRtpPacketLen &&
        HasCorrectRtpVersion(packet) &&
        !PayloadTypeIsReservedForRtcp(packet[1] & 0x7F);
}

bool IsRtcpPacket(rtc::ArrayView<const uint8_t> packet) {
    return packet.size() >= kMinRtcpPacketLen &&
        HasCorrectRtpVersion(packet) &&
        PayloadTypeIsReservedForRtcp(packet[1] & 0x7F);
}

RtpPacketType InferRtpPacketType(rtc::ArrayView<const char> packet) { 
    if (IsRtpPacket(rtc::reinterpret_array_view<const uint8_t>(packet))) {
        return RtpPacketType::kRtp;
    }
    
    if (IsRtcpPacket(rtc::reinterpret_array_view<const uint8_t>(packet))) {
        return RtpPacketType::kRtcp;
    }
 
    return RtpPacketType::kUnknown;
}

uint16_t ParseRtpSequenceNumber(rtc::ArrayView<const uint8_t> packet) {
    return rtc::ByteReader<uint16_t>::ReadBigEndian(packet.data() + 2);
}

uint32_t ParseRtpSsrc(rtc::ArrayView<const uint8_t> packet) {
    return rtc::ByteReader<uint32_t>::ReadBigEndian(packet.data() + 8);
}

bool GetRtcpType(const void* data, size_t len, int* type) {
    if (len < kMinRtcpPacketLen) {
        return false;
    }

    if (!data || !type) {
        return false;
    }

    *type = *((const uint8_t*)data + 1);
    return true;
}

} // namespace xrtc


