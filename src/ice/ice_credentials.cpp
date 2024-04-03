#include "ice/ice_credentials.h"

#include <rtc_base/helpers.h>

#include "ice/ice_def.h"

namespace xrtc {

IceParameters IceCredentials::CreateRandomIceCredentials() {
    return IceParameters(rtc::CreateRandomString(ICE_UFRAG_LENGTH),
            rtc::CreateRandomString(ICE_PWD_LENGTH));
}

} // namespace xrtc


