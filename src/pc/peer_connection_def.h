//
// Created by faker on 23-4-27.
//

#ifndef XRTCSERVER_PEER_CONNECTION_DEF_H
#define XRTCSERVER_PEER_CONNECTION_DEF_H

namespace xrtc {
    enum class PeerConnectionState {
        k_new,
        k_connecting,
        k_connected,
        k_disconnected,
        k_failed,
        k_closed
    };
};


#endif //XRTCSERVER_PEER_CONNECTION_DEF_H
