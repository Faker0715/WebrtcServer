#include "server/rtc_server.h"

#include <unistd.h>

#include <rtc_base/logging.h>
#include <rtc_base/crc32.h>
#include <rtc_base/rtc_certificate_generator.h>
#include <yaml-cpp/yaml.h>

#include "server/rtc_worker.h"

namespace xrtc {

const uint64_t kYearInMs = 365 * 24 * 3600 * 1000L;

void RtcServerRecvNotify(EventLoop* /*el*/, IOWatcher* /*w*/, 
        int fd, int /*events*/, void* data)
{
    int msg;
    if (read(fd, &msg, sizeof(int)) != sizeof(int)) {
        RTC_LOG(LS_WARNING) << "read from pipe error: " << strerror(errno)
            << ", errno: " << errno;
        return;
    }

    RtcServer* server = (RtcServer*)data;
    server->ProcessNotify(msg);
}

RtcServer::RtcServer() :
    el_(new EventLoop(this))
{
}

RtcServer::~RtcServer() {
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

int RtcServer::GenerateAndCheckCertificate() {
    if (!certificate_ || certificate_->HasExpired(time(NULL) * 1000)) {
        rtc::KeyParams key_params;
        RTC_LOG(LS_INFO) << "dtls enabled, key type: " << key_params.type();
        certificate_ = rtc::RTCCertificateGenerator::GenerateCertificate(key_params,
                kYearInMs);
        if (certificate_) {
            rtc::RTCCertificatePEM pem = certificate_->ToPEM();
            RTC_LOG(LS_INFO) << "rtc certificate: \n" << pem.certificate();
        }
    }
    
    if (!certificate_) {
        RTC_LOG(LS_WARNING) << "get certificate error";
        return -1;
    }

    return 0;
}

int RtcServer::Init(const char* conf_file) { 
    if (!conf_file) {
        RTC_LOG(LS_WARNING) << "conf_file is null";
        return -1;
    }

    try {
        YAML::Node config = YAML::LoadFile(conf_file);
        RTC_LOG(LS_INFO) << "rtc server options:\n" << config;
        options_.worker_num = config["worker_num"].as<int>();

    } catch (const YAML::Exception& e) {
        RTC_LOG(LS_WARNING) << "rtc server load conf file error: " << e.msg;
        return -1;
    }
   
    // 生成证书
    if (GenerateAndCheckCertificate() != 0) {
        return -1;
    }

    int fds[2];
    if (pipe(fds)) {
        RTC_LOG(LS_WARNING) << "create pipe error: " << strerror(errno) 
            << ", errno: " << errno;
        return -1;
    }
    
    notify_recv_fd_ = fds[0];
    notify_send_fd_ = fds[1];

    pipe_watcher_ = el_->CreateIOEvent(RtcServerRecvNotify, this);
    el_->StartIOEvent(pipe_watcher_, notify_recv_fd_, EventLoop::READ);
    
    for (int i = 0; i < options_.worker_num; ++i) {
        if (CreateWorker(i) != 0) {
            return -1;
        }
    }

    return 0;
}

int RtcServer::CreateWorker(int worker_id) {
    RTC_LOG(LS_INFO) << "rtc server create worker, worker_id: " << worker_id;
    RtcWorker* worker = new RtcWorker(worker_id, options_);
    
    if (worker->Init() != 0) {
        return -1;
    }

    if (!worker->Start()) {
        return -1;
    }

    workers_.push_back(worker);

    return 0;
}

bool RtcServer::Start() {
    if (thread_) {
        RTC_LOG(LS_WARNING) << "rtc server already start";
        return false;
    }

    thread_ = new std::thread([=]() {
        RTC_LOG(LS_INFO) << "rtc server event loop start";
        el_->Start();
        RTC_LOG(LS_INFO) << "rtc server event loop stop";
    });

    return true;
}

void RtcServer::Stop() {
    Notify(QUIT);
}

int RtcServer::Notify(int msg) {
    int written = write(notify_send_fd_, &msg, sizeof(int));
    return written == sizeof(int) ? 0 : -1;
}

void RtcServer::Join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void RtcServer::PushMsg(std::shared_ptr<RtcMsg> msg) {
    std::unique_lock<std::mutex> lock(q_msg_mtx_);
    q_msg_.push(msg);
}

std::shared_ptr<RtcMsg> RtcServer::PopMsg() {
    std::unique_lock<std::mutex> lock(q_msg_mtx_);
    
    if (q_msg_.empty()) {
        return nullptr;
    }

    std::shared_ptr<RtcMsg> msg = q_msg_.front();
    q_msg_.pop();
    return msg;
}

void RtcServer::InnerStop() {
    el_->DeleteIOEvent(pipe_watcher_);
    el_->Stop();
    close(notify_recv_fd_);
    close(notify_send_fd_);

    for (auto worker : workers_) {
        if (worker) {
            worker->Stop();
            worker->Join();
        }
    }
}

int RtcServer::SendRtcMsg(std::shared_ptr<RtcMsg> msg) {
    PushMsg(msg);
    return Notify(RTC_MSG);
}

RtcWorker* RtcServer::GetWorker(const std::string& stream_name) {
    if (workers_.size() == 0 || workers_.size() != (size_t)options_.worker_num) {
        return nullptr;
    }

    uint32_t num = rtc::ComputeCrc32(stream_name);
    size_t index = num % options_.worker_num;
    return workers_[index];
}

void RtcServer::ProcessRtcMsg() {
    std::shared_ptr<RtcMsg> msg = PopMsg();
    if (!msg) {
        return;
    }
    
    if (GenerateAndCheckCertificate() != 0) {
        return;
    }
    
    msg->certificate = certificate_.get();

    RtcWorker* worker = GetWorker(msg->stream_name);
    if (worker) {
        worker->SendRtcMsg(msg);
    }
}

void RtcServer::ProcessNotify(int msg) {
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


