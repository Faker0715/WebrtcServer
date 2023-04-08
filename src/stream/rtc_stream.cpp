//
// Created by faker on 23-4-2.
//

#include "rtc_stream.h"

namespace xrtc {
    xrtc::RtcStream::RtcStream(EventLoop *el, PortAllocator* allocator, uint64_t uid, const std::string &stream_name, bool audiom, bool video,
                               uint32_t log_id) : el(el), uid(uid), stream_name(stream_name), audio(audiom),
                                                  video(video),
                                                  log_id(log_id), pc(new PeerConnection(el,allocator)) {


    }

    xrtc::RtcStream::~RtcStream() {

    }

    int RtcStream::start(rtc::RTCCertificate *certificate) {
        return pc->init(certificate);
    }
}