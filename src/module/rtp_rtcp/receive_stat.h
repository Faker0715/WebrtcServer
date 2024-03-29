//
// Created by faker on 24-2-3.
//

#ifndef XRTCSERVER_RECEIVE_STAT_H
#define XRTCSERVER_RECEIVE_STAT_H

#include "system_wrappers/include/clock.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include <rtc_base/containers/flat_map.h>
#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"

namespace xrtc {
    class StreamStat {
    public:
        StreamStat(uint32_t ssrc, webrtc::Clock *clock);

        ~StreamStat();

        void UpdateCounters(const webrtc::RtpPacketReceived &packet);
        void MaybeAppendReportBlockAndReset(std::vector<webrtc::rtcp::ReportBlock>& result);
    private:
        bool ReceiveRtpPacket() const {
            return received_seq_first_ >= 0;
        }

        bool UpdateOutOfOrder(const webrtc::RtpPacketReceived &packet,
                              int64_t sequence_number,
                              int64_t now_ms);

        void UpdateJitter(const webrtc::RtpPacketReceived &packet,
                          int64_t receive_time);
    private:
        uint32_t ssrc_;
        webrtc::Clock *clock_;
        webrtc::StreamDataCounters receive_counters_;
        webrtc::Unwrapper<uint16_t> seq_unwrapper_;
        // 累计丢报数 当存在非rtx的重传包，这个值可能是负值
        int32_t cumulative_loss_;
        int64_t received_seq_first_ = -1;
        int64_t received_seq_max_ = -1;
        int max_reordering_threshold_;
        uint32_t last_received_timestamp_ = 0;
        int64_t last_received_time_ms_= 0;
        uint32_t jitter_q4_ = 0;


        int64_t last_report_seq_max_ = -1;
        int32_t last_report_cumulative_loss_ = 0;
        int cumulative_loss_rtcp_offset_ = 0;
        bool cumulative_loss_is_capped_ = false;

        absl::optional<uint16_t> received_seq_out_of_order_;
    };

    class ReceiveStat {
    public:
        ReceiveStat(webrtc::Clock *clock);

        ~ReceiveStat();

        static std::unique_ptr<ReceiveStat> Create(webrtc::Clock *clock);

        void OnRtpPacket(const webrtc::RtpPacketReceived &packet);

        std::vector<webrtc::rtcp::ReportBlock> RtcpReportBlocks(size_t max_blocks);

        // 如果有就创建 没有就返回
        StreamStat *GetOrCreateStat(uint32_t ssrc);

    private:
        webrtc::Clock *clock_;
        // 需求主要是查询
        webrtc::flat_map<uint32_t, std::unique_ptr<StreamStat>> stats_;
        std::vector<uint32_t> all_ssrcs_;
        size_t last_returned_ssrc_idx_ = 0;

    };
}


#endif //XRTCSERVER_RECEIVE_STAT_H
