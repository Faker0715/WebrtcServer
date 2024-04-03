#include "server/signaling_worker.h"

#include <unistd.h>

#include <rtc_base/zmalloc.h>
#include <rtc_base/logging.h>

#include "xrtcserver_def.h"
#include "base/socket.h"
#include "server/tcp_connection.h"
#include "server/rtc_server.h"

extern xrtc::RtcServer* g_rtc_server;

namespace xrtc {

void SignalingWorkerRecvNotify(EventLoop* /*el*/, IOWatcher* /*w*/, int fd, 
        int /*events*/, void *data) 
{
    int msg;
    if (read(fd, &msg, sizeof(int)) != sizeof(int)) {
        RTC_LOG(LS_WARNING) << "read from pipe error: " << strerror(errno)
            << ", errno: " << errno;
        return;
    }

    SignalingWorker* worker = (SignalingWorker*)data;
    worker->ProcessNotify(msg);
}

SignalingWorker::SignalingWorker(int worker_id, const SignalingServerOptions& options) :
    worker_id_(worker_id),
    options_(options),
    el_(new EventLoop(this))
{
}

SignalingWorker::~SignalingWorker() {
    for (auto c : conns_) {
        if (c) {
            CloseConn(c);
        }
    }

    conns_.clear();

    if (el_) {
        delete el_;
        el_ = nullptr;
    }
    
    if (thread_) {
        delete thread_;
        thread_ = nullptr;
    }
}

int SignalingWorker::Init() {
    int fds[2];
    if (pipe(fds)) {
        RTC_LOG(LS_WARNING) << "create pipe error: " << strerror(errno)
            << ", errno: " << errno;
        return -1;
    }

    notify_recv_fd_ = fds[0];
    notify_send_fd_ = fds[1];

    pipe_watcher_ = el_->CreateIOEvent(SignalingWorkerRecvNotify, this);
    el_->StartIOEvent(pipe_watcher_, notify_recv_fd_, EventLoop::READ);

    return 0;
}

bool SignalingWorker::Start() {
    if (thread_) {
        RTC_LOG(LS_WARNING) << "signaling worker already start, worker_id: " << worker_id_;
        return false;
    }

    thread_ = new std::thread([=]() {
        RTC_LOG(LS_INFO) << "signaling worker event loop start, worker_id: " << worker_id_;
        el_->Start();
        RTC_LOG(LS_INFO) << "signaling worker event loop stop, worker_id: " << worker_id_;
    });

    return true;
}

void SignalingWorker::Stop() {
    Notify(SignalingWorker::QUIT);
}

int SignalingWorker::Notify(int msg) {
    int written = write(notify_send_fd_, &msg, sizeof(int));
    return written == sizeof(int) ? 0 : -1;
}

void SignalingWorker::InnerStop() {
    if (!thread_) {
        RTC_LOG(LS_WARNING) << "signaling worker not running, worker_id: " << worker_id_;
        return;
    }

    el_->DeleteIOEvent(pipe_watcher_);
    el_->Stop();

    close(notify_recv_fd_);
    close(notify_send_fd_);
}

void SignalingWorker::ResponseServerOffer(std::shared_ptr<RtcMsg> msg) {
    TcpConnection* c = (TcpConnection*)(msg->conn);
    if (!c) {
        return;
    }

    int fd = msg->fd;
    if (fd <= 0 || size_t(fd) >= conns_.size()) {
        return;
    }
    
    if (conns_[fd] != c) {
        return;
    }

    // 构造响应头
    xhead_t* xh = (xhead_t*)(c->querybuf);
    rtc::Slice header(c->querybuf, XHEAD_SIZE);
    char* buf = (char*)zmalloc(XHEAD_SIZE + MAX_RES_BUF);
    if (!buf) {
        RTC_LOG(LS_WARNING) << "zmalloc error, log_id: " << xh->log_id;
        return;
    }

    memcpy(buf, header.data(), header.size());
    xhead_t* res_xh = (xhead_t*)buf;

    Json::Value res_root;
    res_root["err_no"] = msg->err_no;
    if (msg->err_no != 0) {
        res_root["err_msg"] = "process error";
        res_root["offer"] = "";
    } else {
        res_root["err_msg"] = "success";
        res_root["offer"] = msg->sdp;
    }

    Json::StreamWriterBuilder write_builder;
    write_builder.settings_["indentation"] = "";
    std::string json_data = Json::writeString(write_builder, res_root);
    RTC_LOG(LS_INFO) << "response body: " << json_data;

    res_xh->body_len = json_data.size();
    snprintf(buf + XHEAD_SIZE, MAX_RES_BUF, "%s", json_data.c_str());

    rtc::Slice reply(buf, XHEAD_SIZE + res_xh->body_len);
    AddReply(c, reply);
}

void SignalingWorker::AddReply(TcpConnection* c, const rtc::Slice& reply) {
    c->reply_list.push_back(reply);
    el_->StartIOEvent(c->io_watcher, c->fd, EventLoop::WRITE);
}

void SignalingWorker::ProcessRtcMsg() {
    std::shared_ptr<RtcMsg> msg = PopMsg();
    if (!msg) {
        return;
    }

    switch (msg->cmdno) {
        case CMDNO_PUSH:
        case CMDNO_PULL:
            ResponseServerOffer(msg);
            break;
        default:
            RTC_LOG(LS_WARNING) << "unknown cmdno: " << msg->cmdno
                << ", log_id: " << msg->log_id;
            break;
    }
}

void SignalingWorker::ProcessNotify(int msg) {
    switch (msg) {
        case QUIT:
            InnerStop();
            break;
        case NEW_CONN:
            int fd;
            if (q_conn_.Consume(&fd)) {
                NewConn(fd);
            }
            break;
        case RTC_MSG:
            ProcessRtcMsg();
            break;
        default:
            RTC_LOG(LS_WARNING) << "unknown msg: " << msg;
            break;
    }
}

void SignalingWorker::Join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void ConnIOCb(EventLoop* /*el*/, IOWatcher* /*w*/, int fd, int events, void* data) {
    SignalingWorker* worker = (SignalingWorker*)data;

    if (events & EventLoop::READ) {
        worker->ReadQuery(fd);
    }

    if (events & EventLoop::WRITE) {
        worker->WriteReply(fd);
    }
}

void ConnTimeCb(EventLoop* el, TimerWatcher* /*w*/, void* data) {
    SignalingWorker* worker = (SignalingWorker*)(el->owner());
    TcpConnection* c = (TcpConnection*)data;
    worker->ProcessTimeout(c);
}

void SignalingWorker::WriteReply(int fd) {
    if (fd <= 0 || (size_t)fd >= conns_.size()) {
        return;
    }

    TcpConnection* c = conns_[fd];
    if (!c) {
        return;
    }
    
    while (!c->reply_list.empty()) {
        rtc::Slice reply = c->reply_list.front();
        int nwritten = SockWriteData(c->fd, reply.data() + c->cur_resp_pos,
                reply.size() - c->cur_resp_pos);
        if (-1 == nwritten) {
            CloseConn(c);
            return;
        } else if (0 == nwritten) {
            RTC_LOG(LS_WARNING) << "write zero bytes, fd: " << c->fd
                << ", worker_id: " << worker_id_;
        } else if ((nwritten + c->cur_resp_pos) >= reply.size()) {
            // 写入完成
            c->reply_list.pop_front();
            zfree((void*)reply.data());
            c->cur_resp_pos = 0;
            RTC_LOG(LS_INFO) << "write finished, fd: " << c->fd
                << ", worker_id: " << worker_id_;
        } else {
            c->cur_resp_pos += nwritten;
        }
    }

    c->last_interaction = el_->now();
    if (c->reply_list.empty()) {
        el_->StopIOEvent(c->io_watcher, c->fd, EventLoop::WRITE);
        RTC_LOG(LS_INFO) << "stop write event, fd: " << c->fd
                << ", worker_id: " << worker_id_;
    }
}

void SignalingWorker::ProcessTimeout(TcpConnection* c) {
    if (el_->now() - c->last_interaction >= (unsigned long)options_.connection_timeout) {
        RTC_LOG(LS_INFO) << "connection timeout, fd: " << c->fd; 
        CloseConn(c);
    }
}

void SignalingWorker::NewConn(int fd) {
    RTC_LOG(LS_INFO) << "signaling worker " << worker_id_ << ", receive fd: " << fd;

    if (fd < 0) {
        RTC_LOG(LS_WARNING) << "invalid fd: " << fd;
        return;
    }

    SockSetnonblock(fd);
    SockSetnodelay(fd);

    TcpConnection* c = new TcpConnection(fd);
    SockPeerToStr(fd, c->ip, &(c->port));
    c->io_watcher = el_->CreateIOEvent(ConnIOCb, this);
    el_->StartIOEvent(c->io_watcher, fd, EventLoop::READ);
    
    c->timer_watcher = el_->CreateTimer(ConnTimeCb, c, true);
    el_->StartTimer(c->timer_watcher, 100000); // 100ms
    
    c->last_interaction = el_->now();

    if ((size_t)fd >= conns_.size()) {
        conns_.resize(fd * 2, nullptr);
    }

    conns_[fd] = c;
}

void SignalingWorker::ReadQuery(int fd) {
    RTC_LOG(LS_INFO) << "signaling worker " << worker_id_
        << " receive read event, fd: " << fd;
    
    if (fd < 0 || (size_t)fd >= conns_.size()) {
        RTC_LOG(LS_WARNING) << "invalid fd: " << fd;
        return;
    }

    TcpConnection* c = conns_[fd];
    int nread = 0;
    int read_len = c->bytes_expected;
    int qb_len = sdslen(c->querybuf);
    c->querybuf = sdsMakeRoomFor(c->querybuf, read_len);
    nread = SockReadData(fd, c->querybuf + qb_len, read_len);
    
    c->last_interaction = el_->now();

    RTC_LOG(LS_INFO) << "sock read data, len: " << nread;
    
    if (-1 == nread) {
        CloseConn(c);
        return;
    } else if (nread > 0) {
        sdsIncrLen(c->querybuf, nread);
    }

    int ret = ProcessQueryBuffer(c);
    if (ret != 0) {
        CloseConn(c);
        return;
    }
}

void SignalingWorker::CloseConn(TcpConnection* c) {
    RTC_LOG(LS_INFO) << "close connection, fd: " << c->fd;
    close(c->fd);
    RemoveConn(c);
}

void SignalingWorker::RemoveConn(TcpConnection* c) {
    el_->DeleteTimer(c->timer_watcher);
    el_->DeleteIOEvent(c->io_watcher);
    conns_[c->fd] = nullptr;
    delete c;
}

int SignalingWorker::ProcessQueryBuffer(TcpConnection* c) {
    while (sdslen(c->querybuf) >= c->bytes_processed + c->bytes_expected) {
        xhead_t* head = (xhead_t*)(c->querybuf);
        if (TcpConnection::STATE_HEAD == c->current_state) {
            if (XHEAD_MAGIC_NUM != head->magic_num) {
                RTC_LOG(LS_WARNING) << "invalid data, fd: " << c->fd;
                return -1;
            }

            c->current_state = TcpConnection::STATE_BODY;
            c->bytes_processed = XHEAD_SIZE;
            c->bytes_expected = head->body_len;
        } else {
            rtc::Slice header(c->querybuf, XHEAD_SIZE);
            rtc::Slice body(c->querybuf + XHEAD_SIZE, head->body_len);

            int ret = ProcessRequest(c, header, body);
            if (ret != 0) {
                return -1;
            }

            // 短连接处理
            c->bytes_processed = 65535;
        }
    }

    return 0;
}

int SignalingWorker::ProcessRequest(TcpConnection* c,
        const rtc::Slice& header,
        const rtc::Slice& body)
{
    RTC_LOG(LS_INFO) << "receive body: " << body.data();
   
    xhead_t* xh = (xhead_t*)(header.data());

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    JSONCPP_STRING err;
    reader->parse(body.data(), body.data() + body.size(), &root, &err);
    if (!err.empty()) {
        RTC_LOG(LS_WARNING) << "parse json body error: " << err << ", fd" << c->fd
            << ", log_id: " << xh->log_id;
        return -1;
    }
   
    int ret = 0;

    int cmdno;
    try {
        cmdno = root["cmdno"].asInt();

    } catch (const Json::Exception& e) {
        RTC_LOG(LS_WARNING) << "no cmdno field in body, log_id: " << xh->log_id;
        return -1;
    }
    
    switch (cmdno) {
        case CMDNO_PUSH:
            return ProcessPush(cmdno, c, root, xh->log_id);
        case CMDNO_PULL:
            return ProcessPull(cmdno, c, root, xh->log_id);
        case CMDNO_STOPPUSH:
            ret = ProcessStopPush(cmdno, c, root, xh->log_id);
            break; 
        case CMDNO_STOPPULL:
            ret = ProcessStopPull(cmdno, c, root, xh->log_id);
            break; 
        case CMDNO_ANSWER:
            ret = ProcessAnswer(cmdno, c, root, xh->log_id);
            break;
        default:
            ret = -1;
            RTC_LOG(LS_WARNING) << "unknown cmdno: " << cmdno << ", log_id: " << xh->log_id;
            break;
    }
    
    // 返回处理结果
    char* buf = (char*)zmalloc(XHEAD_SIZE + MAX_RES_BUF);
    xhead_t* res_xh = (xhead_t*)buf;
    memcpy(res_xh, header.data(), header.size());
    
    Json::Value res_root;
    if (0 == ret) {
        res_root["err_no"] = 0;
        res_root["err_msg"] = "success";
    } else {
        res_root["err_no"] = -1;
        res_root["err_msg"] = "process error";
    }
    
    Json::StreamWriterBuilder write_builder;
    write_builder.settings_["indentation"] = "";
    std::string json_data = Json::writeString(write_builder, res_root);
    RTC_LOG(LS_INFO) << "response body: " << json_data;
    
    res_xh->body_len = json_data.size();
    snprintf(buf + XHEAD_SIZE, MAX_RES_BUF, "%s", json_data.c_str());
    
    rtc::Slice reply(buf, XHEAD_SIZE + res_xh->body_len);
    AddReply(c, reply);

    return 0;
}

int SignalingWorker::ProcessPush(int cmdno, TcpConnection* c,
        const Json::Value& root, uint32_t log_id)
{
    uint64_t uid;
    std::string stream_name;
    int audio;
    int video;
    int is_dtls;

    try {
        uid = root["uid"].asUInt64();
        stream_name = root["stream_name"].asString();
        audio = root["audio"].asInt();
        video = root["video"].asInt();
        is_dtls = root["is_dtls"].asInt();
    } catch (const Json::Exception& e) {
        RTC_LOG(LS_WARNING) << "parse json body error: " << e.what()
            << "log_id: " << log_id;
        return -1;
    }
    
    RTC_LOG(LS_INFO) << "cmdno: " << cmdno 
        << " uid: " << uid 
        << " stream_name: " << stream_name 
        << " auido: " << audio 
        << " video: " << video 
        << " is_dtls: " << is_dtls
        << " signaling server push request";
    
    std::shared_ptr<RtcMsg> msg = std::make_shared<RtcMsg>();
    msg->cmdno = cmdno;
    msg->uid = uid;
    msg->stream_name = stream_name;
    msg->audio = audio;
    msg->video = video;
    msg->is_dtls = is_dtls;
    msg->log_id = log_id;
    msg->worker = this;
    msg->conn = c;
    msg->fd = c->fd;

    return g_rtc_server->SendRtcMsg(msg);
}

int SignalingWorker::ProcessPull(int cmdno, TcpConnection* c,
        const Json::Value& root, uint32_t log_id)
{
    uint64_t uid;
    std::string stream_name;
    int audio;
    int video;
    int is_dtls;

    try {
        uid = root["uid"].asUInt64();
        stream_name = root["stream_name"].asString();
        audio = root["audio"].asInt();
        video = root["video"].asInt();
        is_dtls = root["is_dtls"].asInt();
    } catch (const Json::Exception& e) {
        RTC_LOG(LS_WARNING) << "parse json body error: " << e.what()
            << "log_id: " << log_id;
        return -1;
    }
    
    RTC_LOG(LS_INFO) << "cmdno: " << cmdno 
        << " uid: " << uid 
        << " stream_name: " << stream_name 
        << " auido: " << audio 
        << " video: " << video 
        << " is_dtls: " << is_dtls
        << " signaling server pull request";
    
    std::shared_ptr<RtcMsg> msg = std::make_shared<RtcMsg>();
    msg->cmdno = cmdno;
    msg->uid = uid;
    msg->stream_name = stream_name;
    msg->audio = audio;
    msg->video = video;
    msg->is_dtls = is_dtls;
    msg->log_id = log_id;
    msg->worker = this;
    msg->conn = c;
    msg->fd = c->fd;

    return g_rtc_server->SendRtcMsg(msg);
}

int SignalingWorker::ProcessStopPush(int cmdno, TcpConnection* /*c*/,
        const Json::Value& root, uint32_t log_id)
{
    uint64_t uid;
    std::string stream_name;
    
    try {
        uid = root["uid"].asUInt64();
        stream_name = root["stream_name"].asString();
    } catch (const Json::Exception& e) {
        RTC_LOG(LS_WARNING) << "parse json body error: " << e.what()
            << "log_id: " << log_id;
        return -1;
    }
    
    RTC_LOG(LS_INFO) << "cmdno[" << cmdno << "] uid[" << uid 
        << "] stream_name[" << stream_name 
        << "] signaling server send stop push request";
    
    std::shared_ptr<RtcMsg> msg = std::make_shared<RtcMsg>();
    msg->cmdno = cmdno;
    msg->uid = uid;
    msg->stream_name = stream_name;
    msg->log_id = log_id;

    return g_rtc_server->SendRtcMsg(msg);
}

int SignalingWorker::ProcessStopPull(int cmdno, TcpConnection* /*c*/,
        const Json::Value& root, uint32_t log_id)
{
    uint64_t uid;
    std::string stream_name;
    
    try {
        uid = root["uid"].asUInt64();
        stream_name = root["stream_name"].asString();
    } catch (const Json::Exception& e) {
        RTC_LOG(LS_WARNING) << "parse json body error: " << e.what()
            << "log_id: " << log_id;
        return -1;
    }
    
    RTC_LOG(LS_INFO) << "cmdno[" << cmdno << "] uid[" << uid 
        << "] stream_name[" << stream_name 
        << "] signaling server send stop pull request";
    
    std::shared_ptr<RtcMsg> msg = std::make_shared<RtcMsg>();
    msg->cmdno = cmdno;
    msg->uid = uid;
    msg->stream_name = stream_name;
    msg->log_id = log_id;

    return g_rtc_server->SendRtcMsg(msg);
}

int SignalingWorker::ProcessAnswer(int cmdno, TcpConnection* /*c*/,
        const Json::Value& root, uint32_t log_id)
{
    uint64_t uid;
    std::string stream_name;
    std::string answer;
    std::string stream_type;
    
    try {
        uid = root["uid"].asUInt64();
        stream_name = root["stream_name"].asString();
        answer = root["answer"].asString();
        stream_type = root["type"].asString();
    } catch (const Json::Exception& e) {
        RTC_LOG(LS_WARNING) << "parse json body error: " << e.what()
            << "log_id: " << log_id;
        return -1;
    }
    
    RTC_LOG(LS_INFO) << "cmdno[" << cmdno << "] uid[" << uid 
        << "] stream_name[" << stream_name 
        << "] answer[" << answer 
        << "] stream_type[" << stream_type << "] signaling server send answer request";
    
    std::shared_ptr<RtcMsg> msg = std::make_shared<RtcMsg>();
    msg->cmdno = cmdno;
    msg->uid = uid;
    msg->stream_name = stream_name;
    msg->sdp = answer;
    msg->stream_type = stream_type;
    msg->log_id = log_id;

    return g_rtc_server->SendRtcMsg(msg);
}

int SignalingWorker::NotifyNewConn(int fd) {
    q_conn_.Produce(fd);
    return Notify(SignalingWorker::NEW_CONN);
}

void SignalingWorker::PushMsg(std::shared_ptr<RtcMsg> msg) {
    std::unique_lock<std::mutex> lock(q_msg_mtx_);
    q_msg_.push(msg);
}

std::shared_ptr<RtcMsg> SignalingWorker::PopMsg() {
    std::unique_lock<std::mutex> lock(q_msg_mtx_);
    if (q_msg_.empty()) {
        return nullptr;
    }

    std::shared_ptr<RtcMsg> msg = q_msg_.front();
    q_msg_.pop();
    return msg;
}

int SignalingWorker::SendRtcMsg(std::shared_ptr<RtcMsg> msg) {
    PushMsg(msg);
    return Notify(RTC_MSG);
}


} // namespace xrtc


