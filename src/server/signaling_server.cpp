#include "server/signaling_server.h"

#include <unistd.h>

#include <rtc_base/logging.h>
#include <yaml-cpp/yaml.h>

#include "base/socket.h"
#include "server/signaling_worker.h"

namespace xrtc {

void SignalingServerRecvNotify(EventLoop* /*el*/, IOWatcher* /*w*/, 
        int fd, int /*events*/, void* data) 
{
    int msg;
    if (read(fd, &msg, sizeof(int)) != sizeof(int)) {
        RTC_LOG(LS_WARNING) << "read from pipe error: " << strerror(errno)
            << ", errno: " << errno;
        return;
    }

    SignalingServer* server = (SignalingServer*)data;
    server->ProcessNotify(msg);
}

void AcceptNewConn(EventLoop* /*el*/, IOWatcher* /*w*/, int fd, int /*events*/, void* data) {
    int cfd;
    char cip[128];
    int cport;

    cfd = TcpAccept(fd, cip, &cport);
    if (-1 == cfd) {
        return;
    }

    RTC_LOG(LS_INFO) << "accept new conn, fd: " << cfd << ", ip: " << cip
        << ", port: " << cport;

    SignalingServer* server = (SignalingServer*)data;
    server->DispatchNewConn(cfd);
}

SignalingServer::SignalingServer() : el_(new EventLoop(this)) {
}

SignalingServer::~SignalingServer() {
    if (el_) {
        delete el_;
        el_ = nullptr;
    }

    if (thread_) {
        delete thread_;
        thread_ = nullptr;
    }

    for (auto worker : workers_) {
        if (worker) {
            delete worker;
        }
    }

    workers_.clear();
}

int SignalingServer::Init(const char* conf_file) {
    if (!conf_file) {
        RTC_LOG(LS_WARNING) << "signaling server conf_file is null";
        return -1;
    }

    try {
        YAML::Node config = YAML::LoadFile(conf_file);
        RTC_LOG(LS_INFO) << "signaling server options:\n" << config;

        options_.host = config["host"].as<std::string>();
        options_.port = config["port"].as<int>();
        options_.worker_num = config["worker_num"].as<int>();
        options_.connection_timeout = config["connection_timeout"].as<int>();

    } catch (const YAML::Exception& e) {
        RTC_LOG(LS_WARNING) << "catch a YAML exception, line:" << e.mark.line + 1
            << ", column: " << e.mark.column + 1 << ", error: " << e.msg;
        return -1;
    }
    
    // 创建管道，用于线程间通信
    int fds[2];
    if (pipe(fds)) {
        RTC_LOG(LS_WARNING) << "create pipe error: " << strerror(errno) 
            << ", errno: " << errno;
        return -1;
    }
    
    notify_recv_fd_ = fds[0];
    notify_send_fd_ = fds[1];
    // 将recv_fd添加到事件循环，进行管理
    pipe_watcher_ = el_->CreateIOEvent(SignalingServerRecvNotify, this);
    el_->StartIOEvent(pipe_watcher_, notify_recv_fd_, EventLoop::READ);

    // 创建tcp server
    listen_fd_ = CreateTcpServer(options_.host.c_str(), options_.port); 
    if (-1 == listen_fd_) {
        return -1;
    }
    
    io_watcher_ = el_->CreateIOEvent(AcceptNewConn, this); 
    el_->StartIOEvent(io_watcher_, listen_fd_, EventLoop::READ); 
    
    // 创建worker
    for (int i = 0; i < options_.worker_num; ++i) {
        if (CreateWorker(i) != 0) {
            return -1;
        }
    }

    return 0;
}

int SignalingServer::CreateWorker(int worker_id) {
    RTC_LOG(LS_INFO) << "signaling server create worker, worker_id: " << worker_id;
    SignalingWorker* worker = new SignalingWorker(worker_id, options_);

    if (worker->Init() != 0) {
        return -1;
    }

    if (!worker->Start()) {
        return -1;
    }
    
    workers_.push_back(worker);

    return 0;
}

void SignalingServer::DispatchNewConn(int fd) {
    int index = next_worker_index_;
    next_worker_index_++;
    if ((size_t)next_worker_index_ >= workers_.size()) {
        next_worker_index_ = 0;
    }

    SignalingWorker* worker = workers_[index];
    worker->NotifyNewConn(fd);
}

bool SignalingServer::Start() {
    if (thread_) {
        RTC_LOG(LS_WARNING) << "signalling server already start";
        return false;
    }

    thread_ = new std::thread([=]() {
        RTC_LOG(LS_INFO) << "signaling server event loop run";
        el_->Start();
        RTC_LOG(LS_INFO) << "signaling server event loop stop";
    });

    return true;
}

void SignalingServer::Stop() {
    Notify(SignalingServer::QUIT);
}

int SignalingServer::Notify(int msg) {
    int written = write(notify_send_fd_, &msg, sizeof(int));
    return written == sizeof(int) ? 0 : -1;
}

void SignalingServer::ProcessNotify(int msg) {
    switch (msg) {
        case QUIT:
            InnerStop();
            break;
        default:
            RTC_LOG(LS_WARNING) << "unknown msg: " << msg;
            break;
    }
}

void SignalingServer::InnerStop() {
    if (!thread_) {
        RTC_LOG(LS_WARNING) << "signaling server not running";
        return;
    }

    el_->DeleteIOEvent(pipe_watcher_);
    el_->DeleteIOEvent(io_watcher_);
    el_->Stop();

    close(notify_recv_fd_);
    close(notify_send_fd_);
    close(listen_fd_);

    RTC_LOG(LS_INFO) << "signaling server stop";

    for (auto worker : workers_) {
        if (worker) {
            worker->Stop();
            worker->Join();
        }
    }
}

void SignalingServer::Join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

} // namespace xrtc




