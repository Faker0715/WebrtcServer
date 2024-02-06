//
// Created by faker on 24-2-3.
//

#include "receive_stat.h"
#include "rtc_base/logging.h"

namespace xrtc{

    ReceiveStat::ReceiveStat(webrtc::Clock *clock): clock_(clock) {

    }

    ReceiveStat::~ReceiveStat() {

    }

    void ReceiveStat::OnRtpPacket(const webrtc::RtpPacketReceived &packet) {

//        RTC_LOG(LS_INFO) << "ReceiveStat::OnRtpPacket";
        GetOrCreateStat(packet.Ssrc())->UpdateCounters(packet);
    }

    std::unique_ptr<ReceiveStat> ReceiveStat::Create(webrtc::Clock *clock) {
        return std::make_unique<ReceiveStat>(clock);
    }

    StreamStat *ReceiveStat::GetOrCreateStat(uint32_t ssrc) {
        // 如果这个map没有 就插入一个值
        std::unique_ptr<StreamStat>& stat = stats_[ssrc];
        if(nullptr == stat){
            stat = std::make_unique<StreamStat>(ssrc, clock_);
        }
        return stat.get();
    }

    StreamStat::StreamStat(uint32_t ssrc, webrtc::Clock *clock): ssrc_(ssrc), clock_(clock){

    }

    StreamStat::~StreamStat() {

    }

    void StreamStat::UpdateCounters(const webrtc::RtpPacketReceived &packet) {
        int64_t now_ms = clock_->TimeInMilliseconds();

        receive_counters_.transmitted.AddPacket(packet);
        --cumulative_loss;

        int64_t sequence_number = seq_unwrapper_.UnwrapWithoutUpdate(packet.SequenceNumber());
        // 收到第一个包
        if (!ReceiveRtpPacket()){
            received_seq_first_ = sequence_number;
            received_seq_max_ = sequence_number - 1;
        }else if(UpdateOutOfOrder(packet,sequence_number,now_ms)){
            return;
        }
        // 顺序到达rtp
        cumulative_loss += (sequence_number - received_seq_max_);
        received_seq_max_ = sequence_number;
        seq_unwrapper_.UpdateLast(sequence_number);
    }
    bool StreamStat::UpdateOutOfOrder(const webrtc::RtpPacketReceived& packet,
                                      int64_t sequence_number,
                                      int64_t now_ms) {
        // 表示乱序
        return true;

    }
}