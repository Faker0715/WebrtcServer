#ifndef  XRTCSERVER_MODULES_VIDEO_CODING_NACK_REQUESTER_H_
#define  XRTCSERVER_MODULES_VIDEO_CODING_NACK_REQUESTER_H_

#include <set>
#include <map>

#include <rtc_base/numerics/sequence_number_util.h>
#include <rtc_base/third_party/sigslot/sigslot.h>
#include <system_wrappers/include/clock.h>

#include "modules/video_coding/histogram.h"
#include "base/event_loop.h"

namespace xrtc {

class NackRequester {
public:
    NackRequester(webrtc::Clock* clock, EventLoop* el);
    ~NackRequester();
    
    void ProcessNacks();

    int OnReceivedPacket(uint16_t seq_num, bool is_keyframe);
    int OnReceivedPacket(uint16_t seq_num, bool is_keyframe, bool is_recovered);
   
    void ClearUpTo(uint16_t seq_num);
    void UpdateRtt(int64_t rtt_ms);
    sigslot::signal1<const std::vector<uint16_t>&> SignalNackSend;

private:
    enum NackFilterOptions { kSeqNumOnly, kTimeOnly, kSeqNumAndTime };

    struct NackInfo {
        NackInfo();
        NackInfo(uint16_t seq_num,
                uint16_t send_at_seq_num,
                int64_t created_at_time);

        uint16_t seq_num;
        uint16_t send_at_seq_num;
        int64_t created_at_time;
        int64_t sent_at_time;
        int retries;
    }; 
   
    void AddPacketsToNack(uint16_t seq_num_start, uint16_t seq_num_end);
    bool RemovePacketsUntilKeyFrame();
    std::vector<uint16_t> GetNackBatch(NackFilterOptions options);
    void UpdateReorderingStatistics(uint16_t seq_num);
	// Returns how many packets we have to wait in order to receive the packet
	// with probability `probabilty` or higher.
	int WaitNumberOfPackets(float probability) const; 

private:
    webrtc::Clock* const clock_;
    EventLoop* el_;
    TimerWatcher* nack_timer_ = nullptr;
    std::map<uint16_t, NackInfo, webrtc::DescendingSeqNumComp<uint16_t>> nack_list_;
    std::set<uint16_t, webrtc::DescendingSeqNumComp<uint16_t>> keyframe_list_;
    std::set<uint16_t, webrtc::DescendingSeqNumComp<uint16_t>> recovered_list_;
    video_coding::Histogram reordering_histogram_; 
    bool initialized_ = false;
    int64_t rtt_ms_;
    uint16_t newest_seq_num_ = 0;
    const int64_t send_nack_delay_ms_ = 0;
};

} // namespace xrtc

#endif  // XRTCSERVER_MODULES_VIDEO_CODING_NACK_REQUESTER_H_


