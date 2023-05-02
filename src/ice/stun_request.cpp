

#include <rtc_base/helpers.h>
#include <rtc_base/logging.h>
#include <rtc_base/string_encode.h>
#include <rtc_base/time_utils.h>

#include "ice/stun_request.h"

namespace xrtc {

StunRequestManager::~StunRequestManager() {
    while (_requests.begin() != _requests.end()) {
        StunRequest* request = _requests.begin()->second;
        _requests.erase(_requests.begin());
        delete request;
    }
}

void StunRequestManager::send(StunRequest* request) {
    request->set_manager(this);
    request->construct();
    _requests[request->id()] = request;
    request->send();
}

void StunRequestManager::remove(StunRequest* request) {
    auto iter = _requests.find(request->id());
    if (iter != _requests.end()) {
        _requests.erase(iter);
    }
}

bool StunRequestManager::check_response(StunMessage* msg) {
    auto iter = _requests.find(msg->transaction_id());
    if (iter == _requests.end()) {
        return false;
    }
    
    StunRequest* request = iter->second;
    if (msg->type() == get_stun_success_response(request->type())) {
        request->on_request_response(msg);
    } else if (msg->type() == get_stun_error_response(request->type())) {
        request->on_request_error_response(msg);
    } else {
        RTC_LOG(LS_WARNING) << "Received STUN binding response with wrong type=" 
            << msg->type() << ", id=" << rtc::hex_encode(msg->transaction_id());
        delete request;
        return false;
    }

    delete request;
    return true;
}

StunRequest::StunRequest(StunMessage* msg) :
    _msg(msg)
{
    _msg->set_transaction_id(rtc::CreateRandomString(k_stun_transaction_id_length));
}

StunRequest::~StunRequest() {
    if (_manager) {
        _manager->remove(this);
    }

    delete _msg;
    _msg = nullptr;
}

void StunRequest::construct() {
    prepare(_msg);
}

int StunRequest::elapsed() {
    return rtc::TimeMillis() - _ts;
}

void StunRequest::send() {
    _ts = rtc::TimeMillis();
    rtc::ByteBufferWriter buf;
    if (!_msg->write(&buf)) {
        return;
    }
    
    _manager->signal_send_packet(this, buf.Data(), buf.Length());
}

} // namespace xrtc


