//
// Created by faker on 23-4-4.
//

#ifndef XRTCSERVER_ICE_CREDENTIALS_H
#define XRTCSERVER_ICE_CREDENTIALS_H

#include <string>

namespace xrtc{
   struct IceParameters{
       IceParameters() = default;
       IceParameters(const std::string& ufrag,const std::string& pwd):ice_ufrag(ufrag),ice_pwd(pwd){

       };

       std::string ice_ufrag;
       std::string ice_pwd;
   };
   class IceCredentials{
   public:
       static IceParameters create_random_ice_credentials();
   };
}


#endif //XRTCSERVER_ICE_CREDENTIALS_H
