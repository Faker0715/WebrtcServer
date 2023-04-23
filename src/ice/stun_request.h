//
// Created by faker on 23-4-21.
//

#ifndef XRTCSERVER_STUN_REQUEST_H
#define XRTCSERVER_STUN_REQUEST_H

#include "stun.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include <map>

namespace xrtc{
    class StunRequest;
    class StunRequestManager{
    public:
        StunRequestManager() = default;
        ~StunRequestManager() = default;
        void send(StunRequest* request);
        sigslot::signal3<StunRequest*,const char*,size_t> signal_send_packet;
    private:
        typedef std::map<std::string,StunRequest*> RequestMap;
        RequestMap _requests;

    };

    class StunRequest{
    public:
        StunRequest(StunMessage* request);
        virtual ~StunRequest() ;
        const std::string & id() const { return _msg->transaction_id(); }
        void set_manager(StunRequestManager* manager){_manager = manager;}
        void construct();
        void send();
    protected:
        virtual void prepare(StunMessage *msg);
    private:
        StunMessage* _msg;
        StunRequestManager* _manager = nullptr;

    };
}


#endif //XRTCSERVER_STUN_REQUEST_H
