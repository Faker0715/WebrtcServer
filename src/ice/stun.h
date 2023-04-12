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
    const size_t k_stun_message_integrity_size = 20;
    enum StunMessageType{
        STUN_BINDING_REQUEST = 0x0001,
    };
    enum StunAttributeValue{
        STUN_ATTR_USERNAME = 0x0006,
        STUN_ATTR_FINGERPRINT = 0x8028,
        STUN_ATTR_MESSAGE_INTEGRITY = 0x0008,
    };
    enum StunAttributeValueType{
        STUN_VALUE_UNKNOWN = 0,
        STUN_VALUE_UINT32 = 1,
        STUN_VALUE_BYTE_STRING
    };
    class StunByteStringAttribute;
    class StunAttribute;
    class StunMessage {
    public:
        enum class IntegrityStatus{
            k_not_set,
            k_no_integrity,
            k_integtity_ok,
            k_integtity_bad
        };
        StunMessage();
        ~StunMessage();
        int type(){
            return _type;
        }
        size_t length() const{
            return _length;
        }
        static bool vaildate_fingerprint(const char* data,size_t len);
        IntegrityStatus validate_message_integrity(const std::string& password);
        StunAttributeValueType get_attribute_value_type(int type);
        bool read(rtc::ByteBufferReader* buf);
        const StunByteStringAttribute* get_byte_string(uint16_t type);




    private:
        StunAttribute* _create_attribute(uint16_t type, uint16_t length);
        const StunAttribute* _get_attribute(uint16_t type);
        bool _validate_message_integrity_of_type(uint16_t mi_attr_type, size_t mi_attr_size, const char *data, size_t size,
                                            const std::string &password);
    private:

        // 2 + 14
        uint16_t _type;
        uint16_t _length;
        std::string _transaction_id;
        std::vector<std::unique_ptr<StunAttribute>> _attrs;
        IntegrityStatus _integrity = IntegrityStatus::k_not_set;
        std::string _password;
        std::string _buffer;

    };

    class StunAttribute{
    public:
        int type() const { return _type; }
        size_t length() const { return _length; }
        virtual ~StunAttribute();
        static StunAttribute* create(StunAttributeValueType value_type,
                                           uint16_t type,
                                           uint16_t length,
                                           void* owner);

        virtual bool read(rtc::ByteBufferReader* buf) = 0;
    protected:
        StunAttribute(uint16_t type,uint16_t length);
        void comsume_padding(rtc::ByteBufferReader* buf);
    private:
        uint16_t _type;
        uint16_t _length;
    };
    class StunUInt32Attribute:public StunAttribute{
    public:
        static const size_t SIZE = 4;

    };

    class StunByteStringAttribute: public StunAttribute{
    public:
        StunByteStringAttribute(uint16_t type, uint16_t length);
        ~StunByteStringAttribute() override;
        bool read(rtc::ByteBufferReader* buf) override;
        std::string get_string() const{
            return std::string(_bytes,length());
        }
    private:
        char* _bytes = nullptr;

    };
}


#endif //XRTCSERVER_STUN_H
