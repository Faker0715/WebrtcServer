//
// Created by faker on 23-4-4.
//
#include "ice_def.h"

namespace xrtc {
    const int ICE_UFRAG_LENGTH = 4;
    const int ICE_PWD_LENGTH = 24;
    const int STUN_PACKET_SIZE = 60 * 8;
    const int WEAK_PING_INTERVAL = 1000 * STUN_PACKET_SIZE / 10000 ;
    const int STRONG_PING_INTERVAL = 1000 * STUN_PACKET_SIZE / 1000 ;
    const int MIN_PINGS_AT_WEAK_PING_INTERVAL = 3;

    const int STABLEING_CONNECTION_PING_INTERVAL = 900;
    const int STABLE_CONNECTION_PING_INTERVAL = 2500;


}