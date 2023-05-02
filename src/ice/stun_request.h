
#ifndef  __STUN_REQUEST_H_
#define  __STUN_REQUEST_H_

#include <map>

#include <rtc_base/third_party/sigslot/sigslot.h>

#include "ice/stun.h"

namespace xrtc {

class StunRequest;

class StunRequestManager {
public:
    StunRequestManager() = default;
    ~StunRequestManager();

    void send(StunRequest* request);
    void remove(StunRequest* request);
    bool check_response(StunMessage* msg);

    sigslot::signal3<StunRequest*, const char*, size_t> signal_send_packet;

private:
    typedef std::map<std::string, StunRequest*> RequestMap;
    RequestMap _requests;
};

class StunRequest {
public:
    StunRequest(StunMessage* request);
    virtual ~StunRequest();
   
    int type() { return _msg->type(); }
    const std::string& id() { return _msg->transaction_id(); }
    void set_manager(StunRequestManager* manager) { _manager = manager; }
    void construct();
    void send();
    int elapsed();

protected:
    virtual void prepare(StunMessage*) {}
    virtual void on_request_response(StunMessage*) {}
    virtual void on_request_error_response(StunMessage*) {}
    
    friend class StunRequestManager;

private:
    StunMessage* _msg;
    StunRequestManager* _manager = nullptr;
    int64_t _ts = 0;
};

} // namespace xrtc

#endif  //__STUN_REQUEST_H_


