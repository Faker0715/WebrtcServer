#ifndef  __XRTCSERVER_BASE_SOCKET_H_
#define  __XRTCSERVER_BASE_SOCKET_H_

#include <sys/socket.h>

namespace xrtc {

int CreateTcpServer(const char* addr, int port);
int CreateUdpSocket(int family);
int TcpAccept(int sock, char* host, int* port);
int SockSetnonblock(int sock);
int SockSetnodelay(int sock);
int SockPeerToStr(int sock, char* ip, int* port);
int SockReadData(int sock, char* buf, size_t len);
int SockWriteData(int sock, const char* buf, size_t len);
int SockBind(int sock, struct sockaddr* addr, socklen_t len, int min_port, int max_port);
int SockGetAddress(int sock, char* ip, int* port);
int SockRecvFrom(int sock, char* buf, size_t len, struct sockaddr* addr, socklen_t addr_len);
int64_t SockGetRecvTimestamp(int sock);
int SockSendTo(int sock, const char* buf, size_t len, int flag,
        struct sockaddr* addr, socklen_t addr_len);

} // namespace xrtc

#endif  //__XRTCSERVER_BASE_SOCKET_H_


