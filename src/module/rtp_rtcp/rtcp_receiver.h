//
// Created by Faker on 2024/1/30.
//

#ifndef XRTCSERVER_RTCP_RECEIVER_H
#define XRTCSERVER_RTCP_RECEIVER_H

#include "rtp_rtcp_config.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "api/array_view.h"

namespace xrtc{
    class RTCPReceiver{
    public:
        RTCPReceiver(const RtpRtcpConfig& config);
        ~RTCPReceiver();
        void IncomingRtcpPacket(const uint8_t* packet, size_t palength);
        void IncomingRtcpPacket(rtc::ArrayView<const uint8_t> packet);
        void SetRemoteSSRC(uint32_t ssrc){
            remote_ssrc_ = ssrc;
        };
        bool NTP(uint32_t* received_ntp_secs, uint32_t* received_ntp_frac,
                 uint32_t* rtcp_arrival_time_secs, uint32_t* rtcp_arrival_time_frac,
                 uint32_t* rtp_timestamp) const;
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

}


#endif //XRTCSERVER_RTCP_RECEIVER_H
