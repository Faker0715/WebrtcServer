//
// Created by Faker on 2024/1/22.
//

#ifndef XRTCSERVER_RTCP_SENDER_H
#define XRTCSERVER_RTCP_SENDER_H

#include "module/rtp_rtcp/rtp_rtcp_config.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "receive_stat.h"
#include <modules/rtp_rtcp/include/rtp_rtcp_defines.h>
#include <set>
#include <modules/rtp_rtcp/source/time_util.h>
#include <rtc_base/random.h>

namespace xrtc {
    class RTCPSender {
    public:
        struct FeedbackState{
            uint32_t last_rr_ntp_secs = 0; // 记录了最近一次收到sr包时，接收端的ntp时间
            uint32_t last_rr_ntp_frac = 0;
            uint32_t remote_sr = 0; // 从最近的一次SR包中，提取的NTP时间（发送端）
        };
        RTCPSender(const RtpRtcpConfig &config);

        ~RTCPSender();

        int SendRTCP(const FeedbackState& feedback_state, webrtc::RTCPPacketType packet_type);

        void SetRtcpStatus(webrtc::RtcpMode method);

        void SetSendingStatus(bool sending) {
            sending_ = sending;
        };
        uint32_t cur_report_interval_ms() const{return cur_report_interval_ms_;}
    private:
        class PacketSender;
        absl::optional<int32_t> ComputeCompoundRTCPPacket(const FeedbackState& feedback_state,webrtc::RTCPPacketType packet_type,
                                                          PacketSender& sender);

        bool PrepareReport();
        void BuildRR(const FeedbackState& feedback_state,PacketSender& sender);

        std::vector<webrtc::rtcp::ReportBlock>  CreateRtcpReportBlocks(const FeedbackState& feedback_state);
        bool ConsumeFlag(uint32_t type, bool force = false);

        bool IsFlagPresent(uint32_t type);

        void SetFlag(uint32_t type, bool is_valatile);

    private:
        webrtc::Clock *clock_;
        bool audio_;
        uint32_t ssrc_;
        ReceiveStat* receive_stat_;
        webrtc::RtcpMode method_ = webrtc::RtcpMode::kOff;
        bool sending_ = false;
        uint32_t report_interval_ms_;
        uint32_t cur_report_interval_ms_;
        webrtc::Random random_;
        RtpRtcpModuleObserver* rtp_rtcp_module_observer_;

        size_t max_packet_size_;
        struct ReportFlag {
            ReportFlag(uint32_t type, bool is_valatile) : type(type), is_valatile(is_valatile) {}

            bool operator<(const ReportFlag &other) const {
                return type < other.type;
            }

            bool operator==(const ReportFlag &other) const {
                return type == other.type;
            }

            uint32_t type;
            // 不会消费一次被删除
            bool is_valatile;
        };

        // 当前发送的rtcp包的类型
        std::set<ReportFlag> report_flags_;
        typedef void (RTCPSender::*BuilderFunc)(const FeedbackState& feedback_state,PacketSender& sender);
        std::map<uint32_t,BuilderFunc> builders_;
    };
}

#endif //XRTCSERVER_RTCP_SENDER_H
