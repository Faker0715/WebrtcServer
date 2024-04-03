#include "ice/stun_request.h"

#include <rtc_base/helpers.h>
#include <rtc_base/logging.h>
#include <rtc_base/string_encode.h>
#include <rtc_base/time_utils.h>

namespace xrtc {

StunRequestManager::~StunRequestManager() {
    while (requests_.begin() != requests_.end()) {
        StunRequest* request = requests_.begin()->second;
        requests_.erase(requests_.begin());
        delete request;
    }
}

void StunRequestManager::Send(StunRequest* request) {
    request->set_manager(this);
    request->Construct();
    requests_[request->id()] = request;
    request->Send();
}

void StunRequestManager::Remove(StunRequest* request) {
    auto iter = requests_.find(request->id());
    if (iter != requests_.end()) {
        requests_.erase(iter);
    }
}

bool StunRequestManager::CheckResponse(StunMessage* msg) {
    auto iter = requests_.find(msg->transaction_id());
    if (iter == requests_.end()) {
        return false;
    }
    
    StunRequest* request = iter->second;
    if (msg->type() == GetStunSuccessResponse(request->type())) {
        request->OnRequestResponse(msg);
    } else if (msg->type() == GetStunErrorResponse(request->type())) {
        request->OnRequestErrorResponse(msg);
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
    msg_(msg)
{
    msg_->set_transaction_id(rtc::CreateRandomString(kStunTransactionIdLength));
}

StunRequest::~StunRequest() {
    if (manager_) {
        manager_->Remove(this);
    }

    delete msg_;
    msg_ = nullptr;
}

void StunRequest::Construct() {
    Prepare(msg_);
}

int StunRequest::elapsed() {
    return rtc::TimeMillis() - ts_;
}

void StunRequest::Send() {
    ts_ = rtc::TimeMillis();
    rtc::ByteBufferWriter buf;
    if (!msg_->Write(&buf)) {
        return;
    }
    
    manager_->SignalSendPacket(this, buf.Data(), buf.Length());
}

} // namespace xrtc


