#ifndef  __XRTCSERVER_SERVER_SIGNALING_WORKER_H_
#define  __XRTCSERVER_SERVER_SIGNALING_WORKER_H_

#include <thread>
#include <queue>
#include <mutex>

#include <rtc_base/slice.h>
#include <json/json.h>

#include "xrtcserver_def.h"
#include "base/lock_free_queue.h"
#include "base/event_loop.h"
#include "server/signaling_server.h"

namespace xrtc {

class TcpConnection;

class SignalingWorker {
public:
    enum {
        QUIT = 0,
        NEW_CONN = 1,
        RTC_MSG = 2
    };

    SignalingWorker(int worker_id, const SignalingServerOptions& options);
    ~SignalingWorker();
    
    int Init();
    bool Start();
    void Stop();
    int Notify(int msg);
    void Join();
    int NotifyNewConn(int fd);
    void PushMsg(std::shared_ptr<RtcMsg> msg);
    std::shared_ptr<RtcMsg> PopMsg();
    int SendRtcMsg(std::shared_ptr<RtcMsg> msg);

    friend void SignalingWorkerRecvNotify(EventLoop* el, IOWatcher* w, int fd, 
        int events, void *data);

    friend void ConnIOCb(EventLoop*, IOWatcher*, int fd, int events, void* data);
    friend void ConnTimeCb(EventLoop* el, TimerWatcher* /*w*/, void* data);

private:
    void ProcessNotify(int msg);
    void InnerStop();
    void NewConn(int fd);
    void ReadQuery(int fd);
    int ProcessQueryBuffer(TcpConnection* c);
    int ProcessRequest(TcpConnection* c, 
            const rtc::Slice& header,
            const rtc::Slice& body);
    void CloseConn(TcpConnection* c);
    void RemoveConn(TcpConnection* c);
    void ProcessTimeout(TcpConnection* c);
    int ProcessPush(int cmdno, TcpConnection* c,
            const Json::Value& root, uint32_t log_id);
    int ProcessPull(int cmdno, TcpConnection* c,
            const Json::Value& root, uint32_t log_id);
    int ProcessStopPush(int cmdno, TcpConnection* c,
            const Json::Value& root, uint32_t log_id);
    int ProcessStopPull(int cmdno, TcpConnection* c,
            const Json::Value& root, uint32_t log_id);
    int ProcessAnswer(int cmdno, TcpConnection* c,
            const Json::Value& root, uint32_t log_id);
    void ProcessRtcMsg();
    void ResponseServerOffer(std::shared_ptr<RtcMsg> msg);
    void AddReply(TcpConnection* c, const rtc::Slice& reply);
    void WriteReply(int fd);

private:
    int worker_id_;
    SignalingServerOptions options_;
    EventLoop* el_;
    IOWatcher* pipe_watcher_ = nullptr;
    int notify_recv_fd_ = -1;
    int notify_send_fd_ = -1;

    std::thread* thread_ = nullptr;
    LockFreeQueue<int> q_conn_;
    std::vector<TcpConnection*> conns_;

    std::queue<std::shared_ptr<RtcMsg>> q_msg_;
    std::mutex q_msg_mtx_;
};

} // namespace xrtc

#endif  //__XRTCSERVER_SERVER_SIGNALING_WORKER_H_


