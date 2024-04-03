#ifndef  __XRTCSERVER_ICE_ICE_CONNECTION_H_
#define  __XRTCSERVER_ICE_ICE_CONNECTION_H_

#include "base/event_loop.h"
#include "ice/candidate.h"
#include "ice/stun.h"
#include "ice/stun_request.h"
#include "ice/ice_credentials.h"
#include "ice/ice_connection_info.h"

namespace xrtc {

class UDPPort;
class IceConnection;

class ConnectionRequest : public StunRequest {
public:
    ConnectionRequest(IceConnection* conn);
    
protected:
    void Prepare(StunMessage* msg) override;
    void OnRequestResponse(StunMessage* msg) override;
    void OnRequestErrorResponse(StunMessage* msg) override;

private:
    IceConnection* connection_;
};

class IceConnection : public sigslot::has_slots<> {
public:
    enum WriteState {
        STATE_WRITABLE = 0,
        STATE_WRITE_UNRELIABLE = 1,
        STATE_WRITE_INIT = 2,
        STATE_WRITE_TIMEOUT = 3
    };
    
    struct SentPing {
        SentPing(const std::string& id, int64_t ts) :
            id(id), sent_time(ts) {}

        std::string id;
        int64_t sent_time;
    };

    IceConnection(EventLoop* el, UDPPort* port, const Candidate& remote_candidate);
    ~IceConnection();
    
    const Candidate& remote_candidate() const { return remote_candidate_; }
    const Candidate& local_candidate() const;
    UDPPort* port() { return port_; }

    void HandleStunBindingRequest(StunMessage* stun_msg);
    void SendStunBindingResponse(StunMessage* stun_msg); 
    void SendResponseMessage(const StunMessage& response);
    void OnReadPacket(const char* buf, size_t len, int64_t ts);
    void OnConnectionRequestResponse(ConnectionRequest* request, StunMessage* msg);
    void OnConnectionRequestErrorResponse(ConnectionRequest* request, StunMessage* msg);
    void MaybeSetRemoteIceParams(const IceParameters& ice_params);
    void PrintPingsSinceLastResponse(std::string& pings, size_t max);

    void set_write_state(WriteState state);
    WriteState write_state() { return write_state_; }
    bool writable() { return write_state_ == STATE_WRITABLE; }
    bool receiving() { return receiving_; }
    bool weak() { return !(writable() && receiving()); }
    bool active() { return write_state_ != STATE_WRITE_TIMEOUT; }
    bool stable(int64_t now) const;
    void Ping(int64_t now);
    void ReceivedPingResponse(int rtt);
    void UpdateReceiving(int64_t now);
    int ReceivingTimeout();
    uint64_t priority();
    int rtt() { return rtt_; }
    void set_selected(bool value) { selected_ = value; }
    bool selected() { return selected_; }
    void set_state(IceCandidatePairState state);
    IceCandidatePairState state() { return state_; }
    void Destroy();
    void FailAndDestroy();
    void UpdateState(int64_t now);
    int SendPacket(const char* data, size_t len);

    int64_t last_ping_sent() const { return last_ping_sent_; }
    int64_t last_received();
    int num_pings_sent() const { return num_pings_sent_; }

    std::string ToString();
    
    sigslot::signal1<IceConnection*> SignalStateChange;
    sigslot::signal1<IceConnection*> SignalConnectionDestroy;
    sigslot::signal4<IceConnection*, const char*, size_t, int64_t> SignalReadPacket;

private:
    void OnStunSendPacket(StunRequest* request, const char* buf, size_t len);
    bool MissResponse(int64_t now) const;
    bool TooManyPingFails(size_t max_pings, int rtt, int64_t now);
    bool TooLongWithoutResponse(int min_time, int64_t now);

private:
    EventLoop* el_;
    UDPPort* port_;
    Candidate remote_candidate_;

    WriteState write_state_ = STATE_WRITE_INIT;
    bool receiving_ = false;
    bool selected_ = false;

    int64_t last_ping_sent_ = 0;
    int64_t last_ping_received_ = 0;
    int64_t last_ping_response_received_ = 0;
    int64_t last_data_received_ = 0;
    int num_pings_sent_ = 0;
    std::vector<SentPing> pings_since_last_response_;
    StunRequestManager requests_;
    int rtt_ = 3000;
    int rtt_samples_ = 0;
    IceCandidatePairState state_ = IceCandidatePairState::WAITING;
};

} // namespace xrtc

#endif  //__XRTCSERVER_ICE_ICE_CONNECTION_H_


