//
// Created by faker on 23-4-21.
//

#include "stun_request.h"
#include <rtc_base/helpers.h>
namespace xrtc{
    StunRequest::StunRequest(xrtc::StunMessage *msg):_msg(msg){
        _msg->set_transaction_id(rtc::CreateRandomString(k_stun_transaction_id_length));
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