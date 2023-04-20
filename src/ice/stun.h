//
// Created by faker on 23-4-11.
//
#include <string>
#include <memory>
#include <vector>
#include <rtc_base//byte_buffer.h>
#include "rtc_base/socket_address.h"

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
        STUN_BINDING_RESPONSE = 0x0101,
        STUN_BINDING_ERROR_RESPONSE = 0x0111,

    };
    enum StunAttributeValue{
        STUN_ATTR_USERNAME = 0x0006,
        STUN_ATTR_FINGERPRINT = 0x8028,
        STUN_ATTR_ERROR_CODE = 0x0009,
        STUN_ATTR_PRIORITY = 0x0024,
        STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020,
        STUN_ATTR_MESSAGE_INTEGRITY = 0x0008,
    };
    enum StunAttributeValueType{
        STUN_VALUE_UNKNOWN = 0,
        STUN_VALUE_UINT32 = 1,
        STUN_VALUE_BYTE_STRING
    };
    enum StunAddressFamily{
        STUN_ADDRESS_UNDEF = 0,
        STUN_ADDRESS_IPV4 = 1,
        STUN_ADDRESS_IPV6 = 2,
    };
    enum StunErrorCode{
        STUN_ERROR_BAD_REQUEST = 400,
        STUN_ERROR_UNAUTHORIZED = 401,
        STUN_ERROR_SERVER_ERROR = 500,
    };
    extern const char STUN_ERROR_REASON_BAD_REQUEST[];
    extern const char STUN_ERROR_REASON_UNAUTHORIZED[];
    extern const char STUN_ERROR_REASON_SERVER_ERROR[];
    std::string stun_method_to_string(int type);
    class StunByteStringAttribute;
    class StunUInt32Attribute;
    class StunAttribute;
    class StunErrorCodeAttribute;
    class StunMessage {
    public:
        enum class IntegrityStatus {
            k_not_set,
            k_no_integrity,
            k_integrity_ok,
            k_integrity_bad
        };

        StunMessage();
        ~StunMessage();

        int type() const { return _type; }
        void set_type(uint16_t type) { _type = type; }

        size_t length() const { return _length; }
        void set_length(uint16_t length) { _length = length; }

        const std::string& transaction_id() const { return _transaction_id; }
        void set_transaction_id(const std::string& transaction_id) {
            _transaction_id = transaction_id;
        }

        static bool validate_fingerprint(const char* data, size_t len);
        bool add_fingerprint();

        IntegrityStatus validate_message_integrity(const std::string& password);
        bool add_message_integrity(const std::string& password);

        StunAttributeValueType get_attribute_value_type(int type);
        bool read(rtc::ByteBufferReader* buf);
        bool write(rtc::ByteBufferWriter* buf) const;

        void add_attribute(std::unique_ptr<StunAttribute> attr);

        const StunUInt32Attribute* get_uint32(uint16_t type);
        const StunByteStringAttribute* get_byte_string(uint16_t type);

    private:
        StunAttribute* _create_attribute(uint16_t type, uint16_t length);
        const StunAttribute* _get_attribute(uint16_t type);
        bool _validate_message_integrity_of_type(uint16_t mi_attr_type,
                                                 size_t mi_attr_size, const char* data, size_t size,
                                                 const std::string& password);
        bool _add_message_integrity_of_type(uint16_t attr_type,
                                            uint16_t attr_size, const char* key, size_t len);

        // 2 + 14
    private:
        uint16_t _type;
        uint16_t _length;
        std::string _transaction_id;
        std::vector<std::unique_ptr<StunAttribute>> _attrs;
        IntegrityStatus _integrity = IntegrityStatus::k_not_set;
        std::string _password;
        std::string _buffer;
    };

    class StunAttribute {
    public:
        virtual ~StunAttribute();

        int type() const { return _type; }
        void set_type(uint16_t type) { _type = type; }

        size_t length() const { return _length; }
        void set_length(uint16_t length) { _length = length; }

        static StunAttribute* create(StunAttributeValueType value_type,
                                     uint16_t type, uint16_t length, void* owner);

        static std::unique_ptr<StunErrorCodeAttribute> create_error_code();
        virtual bool read(rtc::ByteBufferReader* buf) = 0;
        virtual bool write(rtc::ByteBufferWriter* buf) = 0;

    protected:
        StunAttribute(uint16_t type, uint16_t length);
        void consume_padding(rtc::ByteBufferReader* buf);
        void write_padding(rtc::ByteBufferWriter* buf);

    private:
        uint16_t _type;
        uint16_t _length;
    };

    class StunAddressAttribute : public StunAttribute {
    public:
        static const size_t SIZE_UNDEF = 0;
        static const size_t SIZE_IPV4 = 8;
        static const size_t SIZE_IPV6 = 20;

        StunAddressAttribute(uint16_t type, const rtc::SocketAddress& addr);
        ~StunAddressAttribute() {}

        void set_address(const rtc::SocketAddress& addr);
        StunAddressFamily family();

        bool read(rtc::ByteBufferReader* buf) override;
        bool write(rtc::ByteBufferWriter* buf) override;

    protected:
        rtc::SocketAddress _address;
    };

    class StunXorAddressAttribute : public StunAddressAttribute {
    public:
        StunXorAddressAttribute(uint16_t type, const rtc::SocketAddress& addr);
        ~StunXorAddressAttribute() {}

        bool write(rtc::ByteBufferWriter* buf) override;

    private:
        rtc::IPAddress _get_xored_ip();
    };

    class StunUInt32Attribute:public StunAttribute{
    public:
        static const size_t SIZE = 4;
        StunUInt32Attribute(uint16_t type);
        StunUInt32Attribute(uint16_t type, uint32_t value);
        ~StunUInt32Attribute() override{};
        uint32_t value() const{
            return _bits;
        }
        void set_value(uint32_t value){
            _bits = value;
        }
        bool read(rtc::ByteBufferReader* buf) override;
        bool write(rtc::ByteBufferWriter* buf) override;
    private:
        uint32_t _bits;
    };

    class StunByteStringAttribute: public StunAttribute{
    public:
        StunByteStringAttribute(uint16_t type, uint16_t length);
        StunByteStringAttribute(uint16_t type,const std::string& str);
        ~StunByteStringAttribute() override;
        bool read(rtc::ByteBufferReader* buf) override;
        bool write(rtc::ByteBufferWriter* buf) override;
        std::string get_string() const{
            return std::string(_bytes,length());
        }
        void copy_bytes(const char *bytes, size_t len);
    private:
        void _set_bytes(char *bytes);
    private:
        char* _bytes = nullptr;

    };


    class StunErrorCodeAttribute: public StunAttribute{
    public:
        static const uint16_t MIN_SIZE;
        StunErrorCodeAttribute(uint16_t type,uint16_t length);
        ~StunErrorCodeAttribute() override = default;

        bool read(rtc::ByteBufferReader* buf) override;
        bool write(rtc::ByteBufferWriter* buf) override;
        void set_code(int code);
        void set_reason(const std::string& reason);
    private:
        uint8_t _class;
        uint8_t _number;
        std::string _reason;
    };
}


#endif //XRTCSERVER_STUN_H
