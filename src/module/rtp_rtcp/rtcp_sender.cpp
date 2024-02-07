//
// Created by Faker on 2024/1/22.
//

#include "rtcp_sender.h"
#include "rtc_base/logging.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"

namespace xrtc {
    class RTCPSender::PacketSender {
    public:
        PacketSender(size_t max_packe_size, webrtc::rtcp::RtcpPacket::PacketReadyCallback callback)
                : max_packet_size_(max_packe_size), callback_(callback) {
        }

        ~PacketSender() {};

        void AppendPacket(const webrtc::rtcp::RtcpPacket &packet) {
            packet.Create(buffer_, &index_, max_packet_size_, callback_);
        }

        void Send() {
            if (index_ > 0) {
                callback_(rtc::ArrayView<const uint8_t>(buffer_, index_));
                index_ = 0;
            }
        }

    private:
        size_t max_packet_size_;
        webrtc::rtcp::RtcpPacket::PacketReadyCallback callback_;;
        size_t index_ = 0; // 复合包的索引位置
        uint8_t buffer_[IP_PACKET_SIZE];
    };
    // -28 是为了去掉ip udp 包头
    RTCPSender::RTCPSender(const RtpRtcpConfig &config) : clock_(config.clock_),
                                                          ssrc_(config.local_media_ssrc),
                                                            receive_stat_(config.receive_stat),
                                                          max_packet_size_(IP_PACKET_SIZE - 28) {
        builders_[webrtc::kRtcpReport] = &RTCPSender::BuildRR;
    }

    RTCPSender::~RTCPSender() {

    }

    int RTCPSender::SendRTCP(webrtc::RTCPPacketType packet_type) {
        auto callback = [&](rtc::ArrayView<const uint8_t> packet) {
            RTC_LOG(LS_WARNING) << "====build rtcp packet, size: " << packet.size();
        };
        absl::optional<PacketSender> sender;
        sender.emplace(max_packet_size_, callback);

        auto result = ComputeCompoundRTCPPacket(packet_type,*sender);
        if(result){
            return *result;
        }
        sender->Send();
        return 0;
    }

    absl::optional<int32_t> RTCPSender::ComputeCompoundRTCPPacket(webrtc::RTCPPacketType packet_type,
                                                                  PacketSender &sender) {
        if (method_ == webrtc::RtcpMode::kOff) {
            RTC_LOG(LS_WARNING) << "Can't send rtcp packet when rtcp is off";
        }
        SetFlag(packet_type, false);
        PrepareReport();
        auto it = report_flags_.begin();
        while (it != report_flags_.end()) {
            if (it->is_valatile) {
                it = report_flags_.erase(it);
            } else {
                ++it;
            }
            uint32_t rtcp_packet_type = it->type;
            auto builder_it = builders_.find(rtcp_packet_type);
            if (builder_it == builders_.end()) {
                RTC_LOG(LS_WARNING) << "Can't find builder for rtcp packet type: " << rtcp_packet_type;
            } else {
                BuilderFunc func = builder_it->second;
                (this->*func)(sender);
            }
        }
        return absl::nullopt;

    }

    void RTCPSender::SetRtcpStatus(webrtc::RtcpMode method) {
        method_ = method;
    }

    void RTCPSender::SetFlag(uint32_t type, bool is_valatile) {
        report_flags_.insert(ReportFlag(type, is_valatile));
    }

    bool RTCPSender::PrepareReport() {
        bool generate_report = false;
        if (IsFlagPresent(webrtc::kRtcpSr) || IsFlagPresent(webrtc::kRtcpRr)) {
            generate_report = true;
        } else {
            generate_report = (ConsumeFlag(webrtc::kRtcpReport) && method_ == webrtc::RtcpMode::kReducedSize) ||
                              (method_ == webrtc::RtcpMode::kCompound);
            if (generate_report) {
                SetFlag(sending_ ? webrtc::kRtcpSr : webrtc::kRtcpRr, true);
            }

        }
    }

    bool RTCPSender::ConsumeFlag(uint32_t type, bool force) {
        auto it = report_flags_.find(ReportFlag(type, false));
        if (it != report_flags_.end()) {
            if (it->is_valatile || force) {
                report_flags_.erase(it);
            }
            return true;
        }
        return false;
    }


    bool RTCPSender::IsFlagPresent(uint32_t type) {
        return report_flags_.find(ReportFlag(type, false)) != report_flags_.end();
    }

    void RTCPSender::BuildRR(PacketSender &sender) {
        webrtc::rtcp::ReceiverReport rr;
        rr.SetSenderSsrc(ssrc_);
    }

    std::vector<webrtc::rtcp::ReportBlock> RTCPSender::CreateRtcpReportBlocks() {
        std::vector<webrtc::rtcp::ReportBlock> result;
        if(!receive_stat_){
            return result;
        }
        result = receive_stat_->RtcpReportBlocks(webrtc::RTCP_MAX_REPORT_BLOCKS);
        return result;
    }
}


