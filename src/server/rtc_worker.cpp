#include "server/rtc_worker.h"

#include <unistd.h>

#include <rtc_base/logging.h>

#include "server/signaling_worker.h"

namespace xrtc {

void RtcWorkerRecvNotify(EventLoop* /*el*/, IOWatcher* /*w*/, int fd, 
        int /*events*/, void* data)
{
    int msg;
    if (read(fd, &msg, sizeof(int)) != sizeof(int)) {
        RTC_LOG(LS_WARNING) << "read from pipe error: " << strerror(errno)
            << ", errno: " << errno;
        return;
    }

    RtcWorker* worker = (RtcWorker*)data;
    worker->ProcessNotify(msg);
}

RtcWorker::RtcWorker(int worker_id, const RtcServerOptions& options) :
    options_(options),
    worker_id_(worker_id),
    el_(new EventLoop(this)),
    rtc_stream_mgr_(new RtcStreamManager(el_))
{
}

RtcWorker::~RtcWorker() {
    if (el_) {
        delete el_;
        el_ = nullptr;
    }

    if (thread_) {
        delete thread_;
        thread_ = nullptr;
    }
}

int RtcWorker::Init() {
    int fds[2];
    if (pipe(fds)) {
        RTC_LOG(LS_WARNING) << "create pipe error: " << strerror(errno)
            << ", errno: " << errno;
        return -1;
    }
    
    notify_recv_fd_ = fds[0];
    notify_send_fd_ = fds[1];

    pipe_watcher_ = el_->CreateIOEvent(RtcWorkerRecvNotify, this);
    el_->StartIOEvent(pipe_watcher_, notify_recv_fd_, EventLoop::READ);

    return 0;
}

bool RtcWorker::Start() {
    if (thread_) {
        RTC_LOG(LS_WARNING) << "rtc worker already start, worker_id: " << worker_id_;
        return false;
    }
    
    thread_ = new std::thread([=]() {
        RTC_LOG(LS_INFO) << "rtc worker event loop start, worker_id: " << worker_id_;
        el_->Start();
        RTC_LOG(LS_INFO) << "rtc worker event loop stop, worker_id: " << worker_id_;
    });

    return true;
}

void RtcWorker::Stop() {
    Notify(QUIT);
}

int RtcWorker::Notify(int msg) {
    int written = write(notify_send_fd_, &msg, sizeof(int));
    return written == sizeof(int) ? 0 : -1;
}

void RtcWorker::Join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void RtcWorker::PushMsg(std::shared_ptr<RtcMsg> msg) {
    q_msg_.Produce(msg);
}

bool RtcWorker::PopMsg(std::shared_ptr<RtcMsg>* msg) {
    return q_msg_.Consume(msg);
}

int RtcWorker::SendRtcMsg(std::shared_ptr<RtcMsg> msg) {
    // 将消息投递到worker的队列
    PushMsg(msg);
    return Notify(RTC_MSG);
}

void RtcWorker::InnerStop() {
    if (!thread_) {
        RTC_LOG(LS_WARNING) << "rtc worker not running, worker_id: " << worker_id_;
        return;
    }

    el_->DeleteIOEvent(pipe_watcher_);
    el_->Stop();
    close(notify_recv_fd_);
    close(notify_send_fd_);
}

void RtcWorker::ProcessPush(std::shared_ptr<RtcMsg> msg) {
    std::string offer;
    int ret = rtc_stream_mgr_->CreatePushStream(msg->uid, msg->stream_name,
            msg->audio, msg->video, 
            msg->is_dtls, msg->log_id, 
            (rtc::RTCCertificate*)(msg->certificate),
            offer);
    
    
    RTC_LOG(LS_INFO) << "offer: " << offer;

    msg->sdp = offer;
    if (ret != 0) {
        msg->err_no = -1;
    }

    SignalingWorker* worker = (SignalingWorker*)(msg->worker);
    if (worker) {
        worker->SendRtcMsg(msg);
    }
}

void RtcWorker::ProcessPull(std::shared_ptr<RtcMsg> msg) {
    std::string offer;
    int ret = rtc_stream_mgr_->CreatePullStream(msg->uid, msg->stream_name,
            msg->audio, msg->video, 
            msg->is_dtls, msg->log_id, 
            (rtc::RTCCertificate*)(msg->certificate),
            offer);


    RTC_LOG(LS_INFO) << "offer: " << offer;

    msg->sdp = offer;
    if (ret != 0) {
        msg->err_no = -1;
    }

    SignalingWorker* worker = (SignalingWorker*)(msg->worker);
    if (worker) {
        worker->SendRtcMsg(msg);
    }
}

void RtcWorker::ProcessAnswer(std::shared_ptr<RtcMsg> msg) {
    int ret = rtc_stream_mgr_->SetAnswer(msg->uid, msg->stream_name,
            msg->sdp, msg->stream_type, msg->log_id);
     
    RTC_LOG(LS_INFO) << "rtc worker process answer, uid: " << msg->uid
        << ", stream_name: " << msg->stream_name
        << ", worker_id: " << worker_id_
        << ", log_id: " << msg->log_id
        << ", ret: " << ret;
}

void RtcWorker::ProcessStopPush(std::shared_ptr<RtcMsg> msg) {
    int ret = rtc_stream_mgr_->StopPush(msg->uid, msg->stream_name);
        
    RTC_LOG(LS_INFO) << "rtc worker process stop push, uid: " << msg->uid
        << ", stream_name: " << msg->stream_name
        << ", worker_id: " << worker_id_
        << ", log_id: " << msg->log_id
        << ", ret: " << ret;
}

void RtcWorker::ProcessStopPull(std::shared_ptr<RtcMsg> msg) {
    int ret = rtc_stream_mgr_->StopPull(msg->uid, msg->stream_name);

    RTC_LOG(LS_INFO) << "rtc worker process stop pull, uid: " << msg->uid
        << ", stream_name: " << msg->stream_name
        << ", worker_id: " << worker_id_
        << ", log_id: " << msg->log_id
        << ", ret: " << ret;
}

void RtcWorker::ProcessRtcMsg() {
    std::shared_ptr<RtcMsg> msg;
    if (!PopMsg(&msg)) {
        return;
    }

    RTC_LOG(LS_INFO) << "cmdno[" << msg->cmdno << "] uid[" << msg->uid
        << "] stream_name[" << msg->stream_name << "] audio[" << msg->audio
        << "] video[" << msg->video << "] log_id[" << msg->log_id
        << "] rtc worker receive msg, worker_id: " 
        << worker_id_;

    switch (msg->cmdno) {
        case CMDNO_PUSH:
            ProcessPush(msg);
            break;
        case CMDNO_PULL:
            ProcessPull(msg);
            break;
        case CMDNO_STOPPUSH:
            ProcessStopPush(msg);
            break;
        case CMDNO_STOPPULL:
            ProcessStopPull(msg);
            break;
        case CMDNO_ANSWER:
            ProcessAnswer(msg);
            break;
        default:
            RTC_LOG(LS_WARNING) << "unknown cmdno: " << msg->cmdno 
                << ", log_id: " << msg->log_id;
            break;
    }
}

void RtcWorker::ProcessNotify(int msg) {
    switch (msg) {
        case QUIT:
            InnerStop();
            break;
        case RTC_MSG:
            ProcessRtcMsg();
            break;
        default:
            RTC_LOG(LS_WARNING) << "unknown msg: " << msg;
            break;
    }
}


} // namespace xrtc


