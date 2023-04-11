//
// Created by faker on 23-4-11.
//
#include <string>
#include <memory>
#include <vector>
#ifndef XRTCSERVER_STUN_H
#define XRTCSERVER_STUN_H

namespace xrtc {
    const size_t k_stun_header_size = 20;
    const size_t k_stun_attribute_header_size = 4;
    const size_t k_stun_transaction_id_offset = 8;
    const uint32_t k_stun_magic_cookie = 0x2112A442;
    const size_t k_stun_magic_cookie_length = sizeof(k_stun_magic_cookie);
    enum StunAttributeValue{
        STUN_ATTR_FINGERPRINT = 0x8028,
    };
    class StunAttribute;
    class StunMessage {
    public:
        StunMessage();
        ~StunMessage();
        static bool vaildate_fingerprint(const char* data,size_t len);
    private:

        // 2 + 14
        uint16_t _type;
        uint16_t _length;
        std::string _transaction_id;
        std::vector<std::unique_ptr<StunAttribute>> _attrs;
    };

    class StunAttribute{

    };
    class StunUInt32Attribute:public StunAttribute{
    public:
        static const size_t SIZE = 4;

    };
}


#endif //XRTCSERVER_STUN_H
