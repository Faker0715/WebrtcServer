#ifndef  __RTC_WORKER_H_
#define  __RTC_WORKER_H_

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

    int init();
    bool start();
    void stop();
    int notify(int msg);
    void join();
    void push_msg(std::shared_ptr<RtcMsg> msg);
    bool pop_msg(std::shared_ptr<RtcMsg>* msg);
    int send_rtc_msg(std::shared_ptr<RtcMsg> msg);

    friend void rtc_worker_recv_notify(EventLoop* /*el*/, IOWatcher* /*w*/, int fd, 
        int /*events*/, void* data);

private:
    void _process_notify(int msg);
    void _stop();
    void _process_rtc_msg();
    void _process_push(std::shared_ptr<RtcMsg> msg);
    void _process_pull(std::shared_ptr<RtcMsg> msg);
    void _process_stop_push(std::shared_ptr<RtcMsg> msg);
    void _process_answer(std::shared_ptr<RtcMsg> msg);

private:
    RtcServerOptions _options;
    int _worker_id;
    EventLoop* _el;

    IOWatcher* _pipe_watcher = nullptr;
    int _notify_recv_fd = -1;
    int _notify_send_fd = -1;

    std::thread* _thread = nullptr;
    LockFreeQueue<std::shared_ptr<RtcMsg>> _q_msg;

    std::unique_ptr<RtcStreamManager> _rtc_stream_mgr;
};

} // namespace xrtc


#endif  //__RTC_WORKER_H_


