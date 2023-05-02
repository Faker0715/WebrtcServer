#include <unistd.h>

#include <rtc_base/logging.h>
#include <rtc_base/crc32.h>
#include <rtc_base/rtc_certificate_generator.h>
#include <yaml-cpp/yaml.h>

#include "server/rtc_worker.h"
#include "server/rtc_server.h"

namespace xrtc {

const uint64_t k_year_in_ms = 365 * 24 * 3600 * 1000L;

void rtc_server_recv_notify(EventLoop* /*el*/, IOWatcher* /*w*/, 
        int fd, int /*events*/, void* data)
{
    int msg;
    if (read(fd, &msg, sizeof(int)) != sizeof(int)) {
        RTC_LOG(LS_WARNING) << "read from pipe error: " << strerror(errno)
            << ", errno: " << errno;
        return;
    }

    RtcServer* server = (RtcServer*)data;
    server->_process_notify(msg);
}

RtcServer::RtcServer() :
    _el(new EventLoop(this))
{
}

RtcServer::~RtcServer() {
    if (_el) {
        delete _el;
        _el = nullptr;
    }

    if (_thread) {
        delete _thread;
        _thread = nullptr;
    }

    for (auto worker : _workers) {
        if (worker) {
            delete worker;
        }
    }

    _workers.clear();
}

int RtcServer::_generate_and_check_certificate() {
    if (!_certificate || _certificate->HasExpired(time(NULL) * 1000)) {
        rtc::KeyParams key_params;
        RTC_LOG(LS_INFO) << "dtls enabled, key type: " << key_params.type();
        _certificate = rtc::RTCCertificateGenerator::GenerateCertificate(key_params,
                k_year_in_ms);
        if (_certificate) {
            rtc::RTCCertificatePEM pem = _certificate->ToPEM();
            RTC_LOG(LS_INFO) << "rtc certificate: \n" << pem.certificate();
        }
    }
    
    if (!_certificate) {
        RTC_LOG(LS_WARNING) << "get certificate error";
        return -1;
    }

    return 0;
}

int RtcServer::init(const char* conf_file) { 
    if (!conf_file) {
        RTC_LOG(LS_WARNING) << "conf_file is null";
        return -1;
    }

    try {
        YAML::Node config = YAML::LoadFile(conf_file);
        RTC_LOG(LS_INFO) << "rtc server options:\n" << config;
        _options.worker_num = config["worker_num"].as<int>();

    } catch (YAML::Exception e) {
        RTC_LOG(LS_WARNING) << "rtc server load conf file error: " << e.msg;
        return -1;
    }
   
    // 生成证书
    if (_generate_and_check_certificate() != 0) {
        return -1;
    }

    int fds[2];
    if (pipe(fds)) {
        RTC_LOG(LS_WARNING) << "create pipe error: " << strerror(errno) 
            << ", errno: " << errno;
        return -1;
    }
    
    _notify_recv_fd = fds[0];
    _notify_send_fd = fds[1];

    _pipe_watcher = _el->create_io_event(rtc_server_recv_notify, this);
    _el->start_io_event(_pipe_watcher, _notify_recv_fd, EventLoop::READ);
    
    for (int i = 0; i < _options.worker_num; ++i) {
        if (_create_worker(i) != 0) {
            return -1;
        }
    }

    return 0;
}

int RtcServer::_create_worker(int worker_id) {
    RTC_LOG(LS_INFO) << "rtc server create worker, worker_id: " << worker_id;
    RtcWorker* worker = new RtcWorker(worker_id, _options);
    
    if (worker->init() != 0) {
        return -1;
    }

    if (!worker->start()) {
        return -1;
    }

    _workers.push_back(worker);

    return 0;
}

bool RtcServer::start() {
    if (_thread) {
        RTC_LOG(LS_WARNING) << "rtc server already start";
        return false;
    }

    _thread = new std::thread([=]() {
        RTC_LOG(LS_INFO) << "rtc server event loop start";
        _el->start();
        RTC_LOG(LS_INFO) << "rtc server event loop stop";
    });

    return true;
}

void RtcServer::stop() {
    notify(QUIT);
}

int RtcServer::notify(int msg) {
    int written = write(_notify_send_fd, &msg, sizeof(int));
    return written == sizeof(int) ? 0 : -1;
}

void RtcServer::join() {
    if (_thread && _thread->joinable()) {
        _thread->join();
    }
}

void RtcServer::push_msg(std::shared_ptr<RtcMsg> msg) {
    std::unique_lock<std::mutex> lock(_q_msg_mtx);
    _q_msg.push(msg);
}

std::shared_ptr<RtcMsg> RtcServer::pop_msg() {
    std::unique_lock<std::mutex> lock(_q_msg_mtx);
    
    if (_q_msg.empty()) {
        return nullptr;
    }

    std::shared_ptr<RtcMsg> msg = _q_msg.front();
    _q_msg.pop();
    return msg;
}

void RtcServer::_stop() {
    _el->delete_io_event(_pipe_watcher);
    _el->stop();
    close(_notify_recv_fd);
    close(_notify_send_fd);

    for (auto worker : _workers) {
        if (worker) {
            worker->stop();
            worker->join();
        }
    }
}

int RtcServer::send_rtc_msg(std::shared_ptr<RtcMsg> msg) {
    push_msg(msg);
    return notify(RTC_MSG);
}

RtcWorker* RtcServer::_get_worker(const std::string& stream_name) {
    if (_workers.size() == 0 || _workers.size() != (size_t)_options.worker_num) {
        return nullptr;
    }

    uint32_t num = rtc::ComputeCrc32(stream_name);
    size_t index = num % _options.worker_num;
    return _workers[index];
}

void RtcServer::_process_rtc_msg() {
    std::shared_ptr<RtcMsg> msg = pop_msg();
    if (!msg) {
        return;
    }
    
    if (_generate_and_check_certificate() != 0) {
        return;
    }
    
    msg->certificate = _certificate.get();

    RtcWorker* worker = _get_worker(msg->stream_name);
    if (worker) {
        worker->send_rtc_msg(msg);
    }
}

void RtcServer::_process_notify(int msg) {
    switch (msg) {
        case QUIT:
            _stop();
            break;
        case RTC_MSG:
            _process_rtc_msg();
            break;
        default:
            RTC_LOG(LS_WARNING) << "unknown msg: " << msg;
            break;
    }
}

} // namespace xrtc


