//
// Created by faker on 23-4-4.
//
#include <rtc_base/helpers.h>
#include "ice_credentials.h"
#include "ice/ice_def.h"
namespace xrtc{
    IceParameters xrtc::IceCredentials::create_random_ice_credentials() {

        return IceParameters(rtc::CreateRandomString(ICE_UFRAG_LENGTH),rtc::CreateRandomString(ICE_PWD_LENGTH));

    }
}
