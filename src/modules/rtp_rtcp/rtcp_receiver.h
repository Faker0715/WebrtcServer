#ifndef  __XRTCSERVER_MODULES_RTP_RTCP_RTCP_RECEIVER_H_
#define  __XRTCSERVER_MODULES_RTP_RTCP_RTCP_RECEIVER_H_

#include <api/array_view.h>
#include <modules/rtp_rtcp/source/rtcp_packet/common_header.h>

#include "modules/rtp_rtcp/rtp_rtcp_config.h"

namespace xrtc {

    class RTCPReceiver {
    public:
        RTCPReceiver(const RtpRtcpConfig& config);
        ~RTCPReceiver();

        void IncomingRtcpPacket(const uint8_t* packet, size_t packet_length);
        void IncomingRtcpPacket(rtc::ArrayView<const uint8_t> packet);
        void SetRemoteSsrc(uint32_t ssrc);
        bool NTP(uint32_t* received_ntp_secs,
                 uint32_t* received_ntp_frac,
                 uint32_t* rtcp_arrival_time_secs,
                 uint32_t* rtcp_arrival_time_frac,
                 uint32_t* rtp_timestamp);

    private:
        struct PacketInformation;

        bool ParseCompoundPacket(rtc::ArrayView<const uint8_t> packet,
                                 PacketInformation* packet_information);

        void HandleSr(const webrtc::rtcp::CommonHeader& rtcp_block,
                      PacketInformation* packet_information);
        void HandleRr(const webrtc::rtcp::CommonHeader& rtcp_block,
                      PacketInformation* packet_information);

    private:
        webrtc::Clock* clock_;
        int num_skipped_packets_ = 0;
        uint32_t remote_ssrc_ = 0;
        webrtc::NtpTime remote_sender_ntp_time_;
        uint32_t remote_sender_rtp_time_ = 0;
        webrtc::NtpTime last_received_sr_ntp_;
        uint32_t remote_sender_packet_count_ = 0;
        uint32_t remote_sender_octet_count_ = 0;
    };

} // namespace xrtc


#endif  //__XRTCSERVER_MODULES_RTP_RTCP_RTCP_RECEIVER_H_