//
// Created by faker on 23-4-11.
//

#ifndef XRTCSERVER_ASYNC_UDP_SOCKET_H
#define XRTCSERVER_ASYNC_UDP_SOCKET_H

#include "event_loop.h"
#include <rtc_base/socket_address.h>
#include "socket.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include <list>

namespace xrtc {
    struct UdpPacketData {
    public:
        UdpPacketData(const char *data, size_t size, const rtc::SocketAddress &addr) : _data(new char[size]),
                                                                                       _size(size), _addr(addr) {
            memcpy(_data, data, size);
        }

        ~UdpPacketData() {
            if (_data) {
                delete[] _data;
                _data = nullptr;
            }
        };

        char *data() { return _data; };

        size_t size() const { return _size; }

        const rtc::SocketAddress &addr() {
            return _addr;
        };

    private:
        char *_data;
        size_t _size;
        rtc::SocketAddress _addr;
    };

    class AsyncUdpSocket {
    public:
        AsyncUdpSocket(EventLoop *el, int socket);

        ~AsyncUdpSocket();

        void recv_data();

        void send_data();

        sigslot::signal5<AsyncUdpSocket *, char *, size_t, const rtc::SocketAddress &, int64_t>
                signal_read_packet;

        int send_to(const char *data, size_t size, const rtc::SocketAddress &addr);

    private:
        int _add_udp_packet(const char *data, size_t size, const rtc::SocketAddress &addr);

    private:
        EventLoop *_el;
        int _socket;
        char *_buf;
        size_t _size;
        IOWatcher *_socket_watcher;

        std::list<UdpPacketData *> _udp_packet_list;
    };
}


#endif //XRTCSERVER_ASYNC_UDP_SOCKET_H
