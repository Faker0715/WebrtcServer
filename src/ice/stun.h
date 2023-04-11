//
// Created by faker on 23-4-11.
//
#include <string>
#include <memory>
#include <vector>
#include <rtc_base//byte_buffer.h>
#ifndef XRTCSERVER_STUN_H
#define XRTCSERVER_STUN_H

namespace xrtc {
    const size_t k_stun_header_size = 20;
    const size_t k_stun_attribute_header_size = 4;
    const size_t k_stun_transaction_id_offset = 8;
    const uint32_t k_stun_magic_cookie = 0x2112A442;
    const size_t k_stun_magic_cookie_length = sizeof(k_stun_magic_cookie);
    const size_t k_stun_transaction_id_length = 12;
    enum StunAttributeValue{
        STUN_ATTR_FINGERPRINT = 0x8028,
    };
    class StunAttribute;
    class StunMessage {
    public:
        StunMessage();
        ~StunMessage();
        static bool vaildate_fingerprint(const char* data,size_t len);
        bool read(rtc::ByteBufferReader* buf);
    private:
        std::unique_ptr<StunAttribute> _create_attribute(uint16_t type, uint16_t length);
    private:

        // 2 + 14
        uint16_t _type;
        uint16_t _length;
        std::string _transaction_id;
        std::vector<std::unique_ptr<StunAttribute>> _attrs;
    };

    class StunAttribute{
    public:
        StunAttribute(uint16_t type,uint16_t length):_type(type),_length(length){}
        virtual ~StunAttribute();
        virtual bool read(rtc::ByteBufferReader* buf) = 0;
    private:
        uint16_t _type;
        uint16_t _length;
    };
    class StunUInt32Attribute:public StunAttribute{
    public:
        static const size_t SIZE = 4;

    };
}


#endif //XRTCSERVER_STUN_H
