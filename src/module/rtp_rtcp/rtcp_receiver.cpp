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
    RTCPReceiver::RTCPReceiver(const RtpRtcpConfig &config):
        clock_(config.clock_){
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
        webrtc::rtcp::SenderReport sr;
        if(!sr.Parse(rtcp_block)){
            ++num_skipped_packets_;
            return;
        }
        uint32_t remote_ssrc = sr.sender_ssrc();
        if(remote_ssrc == remote_ssrc_){
            RTC_LOG(LS_WARNING) << "=======sr ssrc: " << sr.sender_ssrc()
                                << ", packet_count: "<< sr.sender_packet_count();
            remote_sender_ntp_time_ = sr.ntp();
            remote_sender_rtp_time_ = sr.rtp_timestamp();
            last_received_sr_ntp_ = clock_->CurrentNtpTime();
            remote_sender_packet_count_ = sr.sender_packet_count();
            remote_sender_octet_count_ = sr.sender_octet_count();
        }
    }

    void RTCPReceiver::HandleRr(const webrtc::rtcp::CommonHeader &rtcp_block,
                                RTCPReceiver::PacketInformation *packet_information) {

    }

    bool RTCPReceiver::NTP(uint32_t *received_ntp_secs, uint32_t *received_ntp_frac, uint32_t *rtcp_arrival_time_secs,
                           uint32_t *rtcp_arrival_time_frac, uint32_t *rtp_timestamp) const {
        if(!last_received_sr_ntp_.Valid()){ //此时还没有收到任何sr包
            return false;
        }
        // SR包中的NTP时间秒数部分
        if(received_ntp_secs){
            *received_ntp_secs = remote_sender_ntp_time_.seconds();
        }
        // SR包中的NTP时间分数部分
        if(received_ntp_frac){
            *received_ntp_frac = remote_sender_ntp_time_.fractions();
        }
        // SR包到达时的本地 NTP 时间
        if(rtcp_arrival_time_secs){
            *rtcp_arrival_time_secs = last_received_sr_ntp_.seconds();
        }
        if(rtcp_arrival_time_frac){
            *rtcp_arrival_time_frac = last_received_sr_ntp_.fractions();
        }
        if(rtp_timestamp){
            *rtp_timestamp = remote_sender_rtp_time_;
        }

        return true;
    }

}
