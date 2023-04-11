//
// Created by faker on 23-4-11.
//

#include "async_udp_socket.h"
#include "rtc_base/logging.h"

namespace xrtc{
    const size_t MAX_BUF_SIZE = 1500;
    void async_udp_socket_io_cb(EventLoop* /*el*/, IOWatcher* /*w*/,int /*fd*/, int events,void* data) {
        AsyncUdpSocket* udp_socket = (AsyncUdpSocket*)data;
        if(EventLoop::READ & events){
            udp_socket->recv_data();
        }
    }
    AsyncUdpSocket::AsyncUdpSocket(EventLoop *el, int socket):_el(el),_socket(socket),_buf(new char[MAX_BUF_SIZE]),_size(MAX_BUF_SIZE) {
        _socket_watcher = _el->create_io_event(async_udp_socket_io_cb, this);
        _el->start_io_event(_socket_watcher, _socket, EventLoop::READ);
    }

    AsyncUdpSocket::~AsyncUdpSocket() {
        if(_socket_watcher){
            _el->delete_io_event(_socket_watcher);
            _socket_watcher = nullptr;
        }
        if(_buf){
            delete[] _buf;
            _buf = nullptr;
        }
    }

    void AsyncUdpSocket::recv_data() {
        while(true){
            struct sockaddr_in addr;
            socklen_t addr_len = sizeof(addr);
            int len = sock_recv_from(_socket,_buf,_size,(struct sockaddr*)&addr,addr_len);
            if(len <= 0){
                return ;
            }
            int64_t ts = sock_get_recv_timestamp(_socket);
            int port = ntohs(addr.sin_port);
            char ip[64] = {0};
            inet_ntop(AF_INET,&addr.sin_addr,ip,sizeof(ip));
            rtc::SocketAddress remote_addr(ip,port);
            signal_read_packet(this,_buf,len,remote_addr,ts);
        }
    }
}