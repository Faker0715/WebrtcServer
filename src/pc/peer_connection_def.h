#ifndef  __PEER_CONNECTION_DEF_H_
#define  __PEER_CONNECTION_DEF_H_

namespace xrtc {

enum class PeerConnectionState {
    k_new,
    k_connecting,
    k_connected,
    k_disconnected,
    k_failed,
    k_closed,
};

} // namespace xrtc

#endif  //__PEER_CONNECTION_DEF_H_


