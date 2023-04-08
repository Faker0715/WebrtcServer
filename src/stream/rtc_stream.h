//
// Created by faker on 23-4-2.
//

#ifndef XRTCSERVER_RTC_STREAM_H
#define XRTCSERVER_RTC_STREAM_H

#include <cstdint>
#include <string>
#include <memory>
#include "base/event_loop.h"
#include "pc/peer_connection.h"
#include "ice/port_allocator.h"
#include <rtc_base/rtc_certificate.h>
namespace xrtc{
    class RtcStream{
    public:
        RtcStream(EventLoop* _el,PortAllocator* allocator, uint64_t,const std::string& stream_name,
                  bool audiom,bool video,uint32_t log_id);
        virtual ~RtcStream();
        int start(rtc::RTCCertificate* certificate);
        virtual std::string create_offer() = 0;
    protected:
        EventLoop* el;
        uint64_t uid;
        std::string stream_name;
        bool audio;
        bool video;
        uint32_t log_id;
        std::unique_ptr<PeerConnection> pc;
    };

}


#endif //XRTCSERVER_RTC_STREAM_H
