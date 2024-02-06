//
// Created by faker on 24-2-3.
//

#ifndef XRTCSERVER_RECEIVE_STAT_H
#define XRTCSERVER_RECEIVE_STAT_H

#include "system_wrappers/include/clock.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include <rtc_base/containers/flat_map.h>
#include "modules/include/module_common_types_public.h"

namespace xrtc{
    class StreamStat{
    public:
        StreamStat(uint32_t ssrc, webrtc::Clock* clock);
        ~StreamStat();
        void UpdateCounters(const webrtc::RtpPacketReceived& packet);

    private:
        bool ReceiveRtpPacket() const{
            return received_seq_first_ >= 0;
        }
        bool UpdateOutOfOrder(const webrtc::RtpPacketReceived& packet,
                                          int64_t sequence_number,
                                          int64_t now_ms);
    private:
        uint32_t ssrc_;
        webrtc::Clock* clock_;
        webrtc::StreamDataCounters receive_counters_;
        webrtc::Unwrapper<uint16_t> seq_unwrapper_;
        // 累计丢报数 当存在非rtx的重传包，这个值可能是负值
        int32_t cumulative_loss;
        int64_t received_seq_first_ = -1;
        int64_t received_seq_max_ = -1;
        int max_reordering_threshold_;
        absl::optional<uint16_t> received_seq_out_of_order_;
    };
    class ReceiveStat{
    public:
        ReceiveStat(webrtc::Clock* clock);
        ~ReceiveStat();
        static std::unique_ptr<ReceiveStat> Create(webrtc::Clock* clock);
        void OnRtpPacket(const webrtc::RtpPacketReceived& packet);
        // 如果有就创建 没有就返回
        StreamStat* GetOrCreateStat(uint32_t ssrc);
    private:
        webrtc::Clock* clock_;
        // 需求主要是查询
        webrtc::flat_map<uint32_t, std::unique_ptr<StreamStat>> stats_;

    };
}


#endif //XRTCSERVER_RECEIVE_STAT_H
