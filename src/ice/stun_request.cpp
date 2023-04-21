//
// Created by faker on 23-4-21.
//

#include "stun_request.h"
namespace xrtc{
    StunRequest::StunRequest(xrtc::StunMessage *msg):_msg(msg){

    }
    StunRequest::~StunRequest() {
    }

    void StunRequest::construct() {
        prepare(_msg);
    }

    void StunRequest::prepare(StunMessage *pMessage) {

    }

    void StunRequestManager::send(StunRequest *request) {
         request->construct();
    }
}