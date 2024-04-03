#ifndef  __XRTCSERVER_BASE_ASYNC_UDP_SOCKET_H_
#define  __XRTCSERVER_BASE_ASYNC_UDP_SOCKET_H_

#include <list>

#include <rtc_base/third_party/sigslot/sigslot.h>
#include <rtc_base/socket_address.h>

#include "base/event_loop.h"

namespace xrtc {

class UdpPacketData {
public:
    UdpPacketData(const char* data, size_t size, const rtc::SocketAddress& addr) :
        data_(new char[size]),
        size_(size),
        addr_(addr)
    {
        memcpy(data_, data, size);
    }
    
    ~UdpPacketData() {
        if (data_) {
            delete[] data_;
            data_ = nullptr;
        }
    }
    
    char* data() { return data_; }
    size_t size() { return size_; }
    const rtc::SocketAddress& addr() { return addr_; }

private:
    char* data_;
    size_t size_;
    rtc::SocketAddress addr_;
};

class AsyncUdpSocket {
public:
    AsyncUdpSocket(EventLoop* el, int socket);
    ~AsyncUdpSocket();
    
    void RecvData();
    void SendData();

    int SendTo(const char* data, size_t size, const rtc::SocketAddress& addr);

    sigslot::signal5<AsyncUdpSocket*, char*, size_t, const rtc::SocketAddress&, int64_t>
        SignalReadPacket;

private:
    int AddUdpPacket(const char* data, size_t size, const rtc::SocketAddress& addr);

private:
    EventLoop* el_;
    int socket_;
    IOWatcher* socket_watcher_;
    char* buf_;
    size_t size_;

    std::list<UdpPacketData*> udp_packet_list_;
};

} // namespace xrtc


#endif  //__XRTCSERVER_BASE_ASYNC_UDP_SOCKET_H_


