//
// Created by Faker on 2024/1/30.
//

#include "rtcp_receiver.h"
#include "rtc_base/logging.h"
#include <modules/rtp_rtcp/source/rtcp_packet/sender_report.h>
#include <modules/rtp_rtcp/source/rtcp_packet/receiver_report.h>
namespace xrtc{

    struct RTCPReceiver::PacketInformation{

    };
    RTCPReceiver::RTCPReceiver(const RtpRtcpConfig &config) {
    }

    RTCPReceiver::~RTCPReceiver() {

    }

    void RTCPReceiver::IncomingRtcpPacket(const uint8_t *packet, size_t palength) {
        IncomingRtcpPacket(rtc::MakeArrayView<const uint8_t>(packet,palength));
    }

    void RTCPReceiver::IncomingRtcpPacket(rtc::ArrayView<const uint8_t> packet) {
        if(packet.empty()){
            RTC_LOG(LS_WARNING) << "====rtcp packet is empty";
            return;
        }
        PacketInformation packet_information;
        if(!ParseCompoundPacket(packet,&packet_information)){
            return;
        }
    }
    bool RTCPReceiver::ParseCompoundPacket(rtc::ArrayView<const uint8_t> packet,
                                           PacketInformation* packet_information) {
        webrtc::rtcp::CommonHeader rtcp_block;
        for(const uint8_t* next_block = packet.begin(); next_block != packet.end(); next_block = rtcp_block.NextPacket()){
            ptrdiff_t remaining_blocks_size = packet.end() - next_block;
            if(!rtcp_block.Parse(next_block,remaining_blocks_size)){
                if(next_block == packet.begin()){
                    RTC_LOG(LS_WARNING) << "invaild incoming rtcp packet";
                    return false;
                }
                ++num_skipped_packets_;
                break;
            }
            switch (rtcp_block.type()) {
                case webrtc::rtcp::SenderReport::kPacketType:
                    HandleSr(rtcp_block,packet_information);
                    break;
                case webrtc::rtcp::ReceiverReport::kPacketType:
                    HandleRr(rtcp_block,packet_information);
                    break;
                default:
                    RTC_LOG(LS_WARNING) << "unknown rtcp packet type: " << rtcp_block.type();
                    ++num_skipped_packets_;
                    break;
            }
        }
        return true;
    }

    void RTCPReceiver::HandleSr(const webrtc::rtcp::CommonHeader &rtcp_block,
                                RTCPReceiver::PacketInformation *packet_information) {

    }

    void RTCPReceiver::HandleRr(const webrtc::rtcp::CommonHeader &rtcp_block,
                                RTCPReceiver::PacketInformation *packet_information) {

    }

}
