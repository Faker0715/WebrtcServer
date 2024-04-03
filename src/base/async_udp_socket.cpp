#include "base/async_udp_socket.h"

#include <rtc_base/logging.h>

#include "base/socket.h"

namespace xrtc {

const size_t MAX_BUF_SIZE = 1500;

void AsyncUdpSocketIOCb(EventLoop* /*el*/, IOWatcher* /*w*/, 
        int /*fd*/, int events, void* data) 
{
    AsyncUdpSocket* udp_socket = (AsyncUdpSocket*)data;
    if (EventLoop::READ & events) {
        udp_socket->RecvData();
    }

    if (EventLoop::WRITE & events) {
        udp_socket->SendData();
    }
}

AsyncUdpSocket::AsyncUdpSocket(EventLoop* el, int socket) :
    el_(el),
    socket_(socket),
    buf_(new char[MAX_BUF_SIZE]),
    size_(MAX_BUF_SIZE)
{
    socket_watcher_ = el_->CreateIOEvent(AsyncUdpSocketIOCb, this);
    el_->StartIOEvent(socket_watcher_, socket_, EventLoop::READ);
}

AsyncUdpSocket::~AsyncUdpSocket() {
    if (socket_watcher_) {
        el_->DeleteIOEvent(socket_watcher_);
        socket_watcher_ = nullptr;
    }

    if (buf_) {
        delete []buf_;
        buf_ = nullptr;
    }
}

void AsyncUdpSocket::RecvData() {
    while (true) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        int len = SockRecvFrom(socket_, buf_, size_, (struct sockaddr*)&addr, addr_len);
        if (len <= 0) {
            return;
        }
        
        int64_t ts = SockGetRecvTimestamp(socket_);
        int port = ntohs(addr.sin_port);
        char ip[64] = {0};
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        rtc::SocketAddress remote_addr(ip, port);
        
        SignalReadPacket(this, buf_, len, remote_addr, ts);
    }
}

void AsyncUdpSocket::SendData() {
    size_t len = 0;
    int sent = 0;
    while (!udp_packet_list_.empty()) {
        // 发送udp packet
        UdpPacketData* packet = udp_packet_list_.front();
        sockaddr_storage saddr;
        len = packet->addr().ToSockAddrStorage(&saddr);
        sent = SockSendTo(socket_, packet->data(), packet->size(), 
                MSG_NOSIGNAL, (struct sockaddr*)&saddr, len);
        if (sent < 0) {
            RTC_LOG(LS_WARNING) << "send udp packet error, remote_addr: " <<
                packet->addr().ToString();
            delete packet;
            udp_packet_list_.pop_front();
            return;
        } else if (0 == sent) {
            RTC_LOG(LS_WARNING) << "send 0 bytes, try again, remote_addr: " <<
                packet->addr().ToString();
            return;
        } else {
            delete packet;
            udp_packet_list_.pop_front();
        }
    }

    if (udp_packet_list_.empty()) {
        el_->StopIOEvent(socket_watcher_, socket_, EventLoop::WRITE);
    }
}

int AsyncUdpSocket::SendTo(const char* data, size_t size, const rtc::SocketAddress& addr) {
    return AddUdpPacket(data, size, addr);
}

int AsyncUdpSocket::AddUdpPacket(const char* data, size_t size,
        const rtc::SocketAddress& addr)
{
    // 尝试发送list里面的数据
    size_t len = 0;
    int sent = 0;
    while (!udp_packet_list_.empty()) {
        // 发送udp packet
        UdpPacketData* packet = udp_packet_list_.front();
        sockaddr_storage saddr;
        len = packet->addr().ToSockAddrStorage(&saddr);
        sent = SockSendTo(socket_, packet->data(), packet->size(), 
                MSG_NOSIGNAL, (struct sockaddr*)&saddr, len);
        if (sent < 0) {
            RTC_LOG(LS_WARNING) << "send udp packet error, remote_addr: " <<
                packet->addr().ToString();
            delete packet;
            udp_packet_list_.pop_front();
            return -1;
        } else if (0 == sent) {
            RTC_LOG(LS_WARNING) << "send 0 bytes, try again, remote_addr: " <<
                packet->addr().ToString();
            goto SEND_AGAIN;
        } else {
            delete packet;
            udp_packet_list_.pop_front();
        }
    }

    // 发送当前数据
    sockaddr_storage saddr;
    len = addr.ToSockAddrStorage(&saddr);
    sent = SockSendTo(socket_, data, size, MSG_NOSIGNAL, (struct sockaddr*)&saddr, len);
    if (sent < 0) {
        RTC_LOG(LS_WARNING) << "send udp packet error, remote_addr: " << addr.ToString();
        return -1;
    } else if (0 == sent) {
        RTC_LOG(LS_WARNING) << "send 0 bytes, try again, remote_addr: " << addr.ToString();
        goto SEND_AGAIN;
    } 
    
    return sent;

SEND_AGAIN: 
    UdpPacketData* packet_data = new UdpPacketData(data, size, addr);
   udp_packet_list_.push_back(packet_data);
   el_->StartIOEvent(socket_watcher_, socket_, EventLoop::WRITE);

    return size;
}

} // namespace xrtc


