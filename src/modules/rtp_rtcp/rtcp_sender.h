#ifndef  __XRTCSERVER_MODULES_RTP_RTCP_RTCP_SENDER_H_
#define  __XRTCSERVER_MODULES_RTP_RTCP_RTCP_SENDER_H_

#include <set>

#include <rtc_base/random.h>
#include <modules/rtp_rtcp/include/rtp_rtcp_defines.h>
#include <modules/rtp_rtcp/source/rtcp_packet/report_block.h>

#include "modules/rtp_rtcp/rtp_rtcp_config.h"
#include "modules/rtp_rtcp/receive_stat.h"

namespace xrtc {

    class RTCPSender {
    public:
        struct FeedbackState {
            uint32_t last_rr_ntp_secs = 0; // 记录了最近一次收到SR包时，接收端的NTP时间
            uint32_t last_rr_ntp_frac = 0;
            uint32_t remote_sr = 0;       // 从最近的一次SR包中，提取的NTP时间
        };

        RTCPSender(const RtpRtcpConfig& config);
        ~RTCPSender();

        int SendRTCP(const FeedbackState& feedback_state,
                     webrtc::RTCPPacketType packet_type,
                     int32_t nack_size = 0,
                     const uint16_t* nack_list = nullptr);
        void SetRtcpStatus(webrtc::RtcpMode method);
        void SetSendingStatus(bool sending) { sending_ = sending; }

        uint32_t cur_report_interval_ms() const { return cur_report_interval_ms_; }

    private:
        class RtcpContext;
        class PacketSender;

        absl::optional<int32_t> ComputeCompoundRTCPPacket(
                const FeedbackState& feedback_state,
                int32_t nack_size,
                const uint16_t* nack_list,
                webrtc::RTCPPacketType packet_type,
                PacketSender& sender);
        void SetFlag(uint32_t type, bool is_volatile);
        bool IsFlagPresent(uint32_t type);
        bool ConsumeFlag(uint32_t type, bool force = false);
        void PrepareReport();

        std::vector<webrtc::rtcp::ReportBlock> CreateRtcpReportBlocks(
                const FeedbackState& feedback_state);

        void BuildRR(const RtcpContext& ctx, PacketSender& sender);

    private:
        webrtc::Clock* clock_;
        bool audio_;
        uint32_t ssrc_;
        ReceiveStat* receive_stat_;
        webrtc::RtcpMode method_ = webrtc::RtcpMode::kOff;
        bool sending_ = false;
        size_t max_packet_size_;
        uint32_t report_interval_ms_;
        uint32_t cur_report_interval_ms_;
        webrtc::Random random_;
        RtpRtcpModuleObserver* rtp_rtcp_module_observer_;

        struct ReportFlag {
            ReportFlag(uint32_t type, bool is_volatile) :
                    type(type), is_volatile(is_volatile) {}

            bool operator<(const ReportFlag& flag) const {
                return type < flag.type;
            }

            bool operator==(const ReportFlag& flag) const {
                return type == flag.type;
            }

            uint32_t type;
            bool is_volatile;
        };

        std::set<ReportFlag> report_flags_;

        typedef void (RTCPSender::*BuilderFunc)(const RtcpContext& ctx,
                                                PacketSender& sender);
        std::map<uint32_t, BuilderFunc> builders_;
    };

} // namespace xrtc

#endif  //__XRTCSERVER_MODULES_RTP_RTCP_RTCP_SENDER_H_