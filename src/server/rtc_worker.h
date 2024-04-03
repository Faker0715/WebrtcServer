#ifndef  __XRTCSERVER_SERVER_RTC_WORKER_H_
#define  __XRTCSERVER_SERVER_RTC_WORKER_H_

#include <thread>

#include "xrtcserver_def.h"
#include "base/lock_free_queue.h"
#include "server/rtc_server.h"
#include "stream/rtc_stream_manager.h"

namespace xrtc {

class RtcWorker {
public:
    enum {
        QUIT = 0,
        RTC_MSG = 1
    };

    RtcWorker(int worker_id, const RtcServerOptions& options);
    ~RtcWorker();

    int Init();
    bool Start();
    void Stop();
    int Notify(int msg);
    void Join();
    void PushMsg(std::shared_ptr<RtcMsg> msg);
    bool PopMsg(std::shared_ptr<RtcMsg>* msg);
    int SendRtcMsg(std::shared_ptr<RtcMsg> msg);

    friend void RtcWorkerRecvNotify(EventLoop* /*el*/, IOWatcher* /*w*/, int fd, 
        int /*events*/, void* data);

private:
    void ProcessNotify(int msg);
    void InnerStop();
    void ProcessRtcMsg();
    void ProcessPush(std::shared_ptr<RtcMsg> msg);
    void ProcessPull(std::shared_ptr<RtcMsg> msg);
    void ProcessStopPush(std::shared_ptr<RtcMsg> msg);
    void ProcessStopPull(std::shared_ptr<RtcMsg> msg);
    void ProcessAnswer(std::shared_ptr<RtcMsg> msg);

private:
    RtcServerOptions options_;
    int worker_id_;
    EventLoop* el_;

    IOWatcher* pipe_watcher_ = nullptr;
    int notify_recv_fd_ = -1;
    int notify_send_fd_ = -1;

    std::thread* thread_ = nullptr;
    LockFreeQueue<std::shared_ptr<RtcMsg>> q_msg_;

    std::unique_ptr<RtcStreamManager> rtc_stream_mgr_;
};

} // namespace xrtc


#endif  //__XRTCSERVER_SERVER_RTC_WORKER_H_


