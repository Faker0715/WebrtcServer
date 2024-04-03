
#ifndef  __XRTCSERVER_ICE_ICE_DEF_H_
#define  __XRTCSERVER_ICE_ICE_DEF_H_

namespace xrtc {

#define LOCAL_PORT_TYPE "host"
#define PRFLX_PORT_TYPE "prflx"

extern const int ICE_UFRAG_LENGTH;
extern const int ICE_PWD_LENGTH;

extern const int WEAK_PING_INTERVAL;
extern const int STRONG_PING_INTERVAL;
extern const int STABLING_CONNECTION_PING_INTERVAL;
extern const int STABLE_CONNECTION_PING_INTERVAL;
extern const int MIN_PINGS_AT_WEAK_PING_INTERVAL;
extern const int WEAK_CONNECTION_RECEIVE_TIMEOUT;
extern const int CONNECTION_WRITE_CONNECT_FAILS;
extern const int CONNECTION_WRITE_CONNECT_TIMEOUT;
extern const int CONNECTION_WRITE_TIMEOUT;

enum IceCandidateComponent {
    RTP = 1,
    RTCP = 2
};

enum IcePriorityValue {
    ICE_TYPE_PREFERENCE_RELAY_UDP = 2,
    ICE_TYPE_PREFERENCE_SRFLX = 100,
    ICE_TYPE_PREFERENCE_PRFLX = 110,
    ICE_TYPE_PREFERENCE_HOST = 126,
};

};

#endif  //__XRTCSERVER_ICE_ICE_DEF_H_


