//
// Created by faker on 23-4-21.
//

#ifndef XRTCSERVER_STUN_REQUEST_H
#define XRTCSERVER_STUN_REQUEST_H

#include "stun.h"

namespace xrtc{
    class StunRequest;
    class StunRequestManager{
    public:
        StunRequestManager() = default;
        ~StunRequestManager() = default;
        void send(StunRequest* request);
    };

    class StunRequest{
    public:
        StunRequest(StunMessage* request);
        virtual ~StunRequest() ;
        const std::string & id() const { return _msg->transaction_id(); }
        void construct();
    protected:
        virtual void prepare(StunMessage *msg);
    private:
        StunMessage* _msg;

    };
}


#endif //XRTCSERVER_STUN_REQUEST_H
