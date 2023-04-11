//
// Created by faker on 23-4-11.
//

#ifndef XRTCSERVER_ASYNC_UDP_SOCKET_H
#define XRTCSERVER_ASYNC_UDP_SOCKET_H

#include "event_loop.h"
#include <rtc_base/socket_address.h>
#include "socket.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace xrtc{
    class AsyncUdpSocket{
    public:
        AsyncUdpSocket(EventLoop* el,int socket);
        ~AsyncUdpSocket();
        void recv_data();
        sigslot::signal5<AsyncUdpSocket*,char*,size_t,const rtc::SocketAddress&,int64_t>
                signal_read_packet;
    private:
        EventLoop* _el;
        int _socket;
        char* _buf;
        size_t _size;
        IOWatcher* _socket_watcher;
    };
}


#endif //XRTCSERVER_ASYNC_UDP_SOCKET_H
