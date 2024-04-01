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
    private:
        struct PacketInformation;
        bool ParseCompoundPacket(rtc::ArrayView<const uint8_t> packet,
                                 PacketInformation* packet_information);
        void HandleSr(const webrtc::rtcp::CommonHeader& rtcp_block,
                      PacketInformation* packet_information);
        void HandleRr(const webrtc::rtcp::CommonHeader& rtcp_block,
                        PacketInformation* packet_information);
    private:
        int num_skipped_packets_ = 0;
        uint32_t remote_ssrc_ = 0;
    };

}


#endif //XRTCSERVER_RTCP_RECEIVER_H
