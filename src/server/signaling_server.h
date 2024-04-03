#ifndef  __XRTCSERVER_SERVER_SIGNALING_SERVER_H_
#define  __XRTCSERVER_SERVER_SIGNALING_SERVER_H_

#include <string>
#include <thread>
#include <vector>

#include "base/event_loop.h"

namespace xrtc {

class SignalingWorker;

struct SignalingServerOptions {
    std::string host;
    int port;
    int worker_num;
    int connection_timeout;
};

class SignalingServer {
public:
    enum {
        QUIT = 0
    };

    SignalingServer();
    ~SignalingServer();
    
    int Init(const char* conf_file);
    bool Start();
    void Stop();
    int Notify(int msg);
    void Join();

    friend void SignalingServerRecvNotify(EventLoop* el, IOWatcher* w, 
        int fd, int events, void* data);

    friend void AcceptNewConn(EventLoop* el, IOWatcher* w, 
            int fd, int events, void* data);

private:
    void ProcessNotify(int msg);    
    void InnerStop();
    int CreateWorker(int worker_id);
    void DispatchNewConn(int fd);

private:
    SignalingServerOptions options_;
    EventLoop* el_;
    IOWatcher* io_watcher_ = nullptr;
    IOWatcher* pipe_watcher_ = nullptr;
    int notify_recv_fd_ = -1;
    int notify_send_fd_ = -1;
    std::thread* thread_ = nullptr;
    
    int listen_fd_ = -1;
    std::vector<SignalingWorker*> workers_;
    int next_worker_index_ = 0;
};


} // namespace xrtc


#endif  //__XRTCSERVER_SERVER_SIGNALING_SERVER_H_


