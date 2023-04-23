//
// Created by faker on 23-4-21.
//

#include "stun_request.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
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
         request->set_manager(this);
         request->construct();
         _requests[request->id()] = request;
         request->send();
    }

    bool StunRequestManager::check_response(StunMessage *msg) {
        auto iter = _requests.find(msg->transaction_id());
        if(iter == _requests.end()){
            return false;
        }
        StunRequest* request = iter->second;
        if(msg->type() == get_stun_success_response(request->type())){
           request->on_request_response(msg);
        }else if(msg->type() == get_stun_error_response(request->type())){
           request->on_error_request_response(msg);
        }else{
            RTC_LOG(LS_WARNING) << "Received STUN binding response with wrong type: "
                << msg->type() << ", id=" << rtc::hex_encode(msg->transaction_id());
            return false;
        }
        return true;
    }

    void StunRequest::send(){
        _ts = rtc::TimeMillis();
        rtc::ByteBufferWriter buf;
        if(!_msg->write(&buf)){
            return;
        }

        _manager->signal_send_packet(this,buf.Data(),buf.Length());

    }

    int StunRequest::elapsed() {
        return rtc::TimeMillis() - _ts;
    }
}