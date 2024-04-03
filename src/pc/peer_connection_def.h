#ifndef  __XRTCSERVER_PC_PEER_CONNECTION_DEF_H_
#define  __XRTCSERVER_PC_PEER_CONNECTION_DEF_H_

namespace xrtc {

enum class PeerConnectionState {
    kNew,
    kConnecting,
    kConnected,
    kDisconnected,
    kFailed,
    kClosed,
};

} // namespace xrtc

#endif  //__XRTCSERVER_PC_PEER_CONNECTION_DEF_H_


