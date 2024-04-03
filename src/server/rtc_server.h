#ifndef  __XRTCSERVER_SERVER_RTC_SERVER_H_
#define  __XRTCSERVER_SERVER_RTC_SERVER_H_

#include <thread>
#include <queue>
#include <mutex>

#include <rtc_base/rtc_certificate.h>

#include "xrtcserver_def.h"
#include "base/event_loop.h"

namespace xrtc {

struct RtcServerOptions {
    int worker_num;
};

class RtcWorker;

class RtcServer {
public:
    enum {
        QUIT = 0,
        RTC_MSG = 1
    };

    RtcServer();
    ~RtcServer();
    
    int Init(const char* conf_file);
    bool Start();
    void Stop();
    int Notify(int msg);
    void Join();
    int SendRtcMsg(std::shared_ptr<RtcMsg>);
    void PushMsg(std::shared_ptr<RtcMsg>);
    std::shared_ptr<RtcMsg> PopMsg();

    friend void RtcServerRecvNotify(EventLoop*, IOWatcher*, int, int, void*);

private:
    void ProcessNotify(int msg);
    void InnerStop();
    void ProcessRtcMsg();
    int CreateWorker(int worker_id);
    RtcWorker* GetWorker(const std::string& stream_name);
    int GenerateAndCheckCertificate();

private:
    EventLoop* el_;
    RtcServerOptions options_;
    
    std::thread* thread_ = nullptr;

    IOWatcher* pipe_watcher_ = nullptr;
    int notify_recv_fd_ = -1;
    int notify_send_fd_ = -1;

    std::queue<std::shared_ptr<RtcMsg>> q_msg_;
    std::mutex q_msg_mtx_;

    std::vector<RtcWorker*> workers_;
    rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
};

} // namespace xrtc

#endif  //__XRTCSERVER_SERVER_RTC_SERVER_H_


