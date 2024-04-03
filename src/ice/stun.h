#ifndef  __XRTCSERVER_ICE_ICE_STUN_H_
#define  __XRTCSERVER_ICE_ICE_STUN_H_

#include <string>
#include <memory>
#include <vector>

#include <rtc_base/socket_address.h>
#include <rtc_base/byte_buffer.h>

namespace xrtc {

const size_t kStunHeaderSize = 20;
const size_t kStunAttributeHeaderSize = 4;
const size_t kStunTransactionIdOffset = 8;
const size_t kStunTransactionIdLength = 12;
const uint32_t kStunMagicCookie = 0x2112A442;
const size_t kStunMagicCookieLength = sizeof(kStunMagicCookie);
const size_t kStunMessageIntegritySize = 20;
const uint32_t kStunTypeMask = 0x0110;

enum StunMessageType {
    STUN_BINDING_REQUEST = 0x0001,
    STUN_BINDING_RESPONSE = 0x0101,
    STUN_BINDING_ERROR_RESPONSE = 0x0111,
};

enum StunAttributeType {
    STUN_ATTR_USERNAME = 0x0006,
    STUN_ATTR_MESSAGE_INTEGRITY = 0x0008,
    STUN_ATTR_ERROR_CODE = 0x0009,
    STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020,
    STUN_ATTR_PRIORITY = 0x0024,
    STUN_ATTR_USE_CANDIDATE = 0x0025,
    STUN_ATTR_FINGERPRINT = 0x8028,
    STUN_ATTR_ICE_CONTROLLING = 0x802A,
};

enum StunAttributeValueType {
    STUN_VALUE_UNKNOWN = 0,
    STUN_VALUE_UINT32,
    STUN_VALUE_BYTE_STRING,
};

enum StunErrorCode {
    STUN_ERROR_BAD_REQUEST = 400,
    STUN_ERROR_UNAUTHORIZED = 401,
    STUN_ERROR_UNKNOWN_ATTRIBUTE = 420,
    STUN_ERROR_SERVER_ERROR = 500,
    STUN_ERROR_GLOBAL_FAIL = 600,
};

enum StunAddressFamily {
    STUN_ADDRESS_UNDEF = 0,
    STUN_ADDRESS_IPV4 = 1,
    STUN_ADDRESS_IPV6 = 2,
};

extern const char STUN_ERROR_REASON_BAD_REQUEST[];
extern const char STUN_ERROR_REASON_UNAUTHORIZED[];
extern const char STUN_ERROR_REASON_SERVER_ERROR[];

class StunAttribute;
class StunUInt32Attribute;
class StunByteStringAttribute;
class StunErrorCodeAttribute;

std::string StunMethodToString(int type);

class StunMessage {
public:
    enum class IntegrityStatus {
        kNotSet,
        kNoIntegrity,
        kIntegrityOk,
        kIntegrityBad
    };

    StunMessage();
    ~StunMessage();
   
    int type() const { return type_; }
    void set_type(uint16_t type) { type_ = type; }

    size_t length() const { return length_; }
    void set_length(uint16_t length) { length_ = length; }

    const std::string& transaction_id() const { return transaction_id_; }
    void set_transaction_id(const std::string& transaction_id) {
        transaction_id_ = transaction_id;
    }

    static bool ValidateFingerprint(const char* data, size_t len);
    bool AddFingerprint();

    IntegrityStatus ValidateMessageIntegrity(const std::string& password);
    bool AddMessageIntegrity(const std::string& password);
    IntegrityStatus integrity() { return integrity_; }
    bool IntegrityOk() { return integrity_ == IntegrityStatus::kIntegrityOk; }

    StunAttributeValueType GetAttributeValueType(int type);
    bool Read(rtc::ByteBufferReader* buf);
    bool Write(rtc::ByteBufferWriter* buf) const;
    
    void AddAttribute(std::unique_ptr<StunAttribute> attr);

    const StunUInt32Attribute* GetUInt32(uint16_t type);
    const StunByteStringAttribute* GetByteString(uint16_t type);
    const StunErrorCodeAttribute* GetErrorCode();
    int GetErrorCodeValue();

private:
    StunAttribute* CreateAttribute(uint16_t type, uint16_t length);
    const StunAttribute* GetAttribute(uint16_t type);
    bool ValidateMessageIntegrityOfType(uint16_t mi_attr_type,
            size_t mi_attr_size, const char* data, size_t size,
            const std::string& password);
    bool AddMessageIntegrityOfType(uint16_t attr_type,
            uint16_t attr_size, const char* key, size_t len);

private:
    uint16_t type_;
    uint16_t length_;
    std::string transaction_id_;
    std::vector<std::unique_ptr<StunAttribute>> attrs_;
    IntegrityStatus integrity_ = IntegrityStatus::kNotSet;
    std::string password_;
    std::string buffer_;
};

class StunAttribute {
public:
    virtual ~StunAttribute();
   
    int type() const { return type_; }
    void set_type(uint16_t type) { type_ = type; }

    size_t length() const { return length_; }
    void set_length(uint16_t length) { length_ = length; }

    static StunAttribute* Create(StunAttributeValueType value_type,
            uint16_t type, uint16_t length, void* owner);
    static std::unique_ptr<StunErrorCodeAttribute> CreateErrorCode();

    virtual bool Read(rtc::ByteBufferReader* buf) = 0;
    virtual bool Write(rtc::ByteBufferWriter* buf) = 0;
   
protected:
    StunAttribute(uint16_t type, uint16_t length);
    void ConsumePadding(rtc::ByteBufferReader* buf);
    void WritePadding(rtc::ByteBufferWriter* buf);

private:
    uint16_t type_;
    uint16_t length_;
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

    bool Read(rtc::ByteBufferReader* buf) override;
    bool Write(rtc::ByteBufferWriter* buf) override;

protected:
    rtc::SocketAddress address_;
};

class StunXorAddressAttribute : public StunAddressAttribute {
public:
    StunXorAddressAttribute(uint16_t type, const rtc::SocketAddress& addr);
    ~StunXorAddressAttribute() {}

    bool Write(rtc::ByteBufferWriter* buf) override;

private:
    rtc::IPAddress GetXoredIP();
};

class StunUInt32Attribute : public StunAttribute {
public:
    static const size_t SIZE = 4;
    StunUInt32Attribute(uint16_t type);
    StunUInt32Attribute(uint16_t type, uint32_t value);
    ~StunUInt32Attribute() override {}
   
    uint32_t value() const { return bits_; }
    void set_value(uint32_t value) { bits_ = value; }
    
    bool Read(rtc::ByteBufferReader* buf) override;
    bool Write(rtc::ByteBufferWriter* buf) override;

private:
    uint32_t bits_;
};

class StunUInt64Attribute : public StunAttribute {
public:
    static const size_t SIZE = 8;
    StunUInt64Attribute(uint16_t type);
    StunUInt64Attribute(uint16_t type, uint64_t value);
    ~StunUInt64Attribute() override {}
   
    uint64_t value() const { return bits_; }
    void set_value(uint64_t value) { bits_ = value; }
    
    bool Read(rtc::ByteBufferReader* buf) override;
    bool Write(rtc::ByteBufferWriter* buf) override;

private:
    uint64_t bits_;
};

class StunByteStringAttribute : public StunAttribute {
public:
    StunByteStringAttribute(uint16_t type, uint16_t length);
    StunByteStringAttribute(uint16_t type, const std::string& str);
    ~StunByteStringAttribute() override;
   
    void CopyBytes(const char* bytes, size_t len);

    bool Read(rtc::ByteBufferReader* buf) override;
    bool Write(rtc::ByteBufferWriter* buf) override;
    
    std::string GetString() const { return std::string(bytes_, length()); }

private:
    void set_bytes(char* bytes);

private:
    char* bytes_ = nullptr;
};

class StunErrorCodeAttribute : public StunAttribute {
public:
    static const uint16_t MIN_SIZE;
    StunErrorCodeAttribute(uint16_t type, uint16_t length);
    ~StunErrorCodeAttribute() override = default;
    
    void set_code(int code);
    int code() const;
    void set_reason(const std::string& reason);
    
    bool Read(rtc::ByteBufferReader* buf) override;
    bool Write(rtc::ByteBufferWriter* buf) override;

private:
    uint8_t class_;
    uint8_t number_;
    std::string reason_;
};

int GetStunSuccessResponse(int req_type);
int GetStunErrorResponse(int req_type);
bool IsStunRequestType(int req_type);

} // namespace xrtc


#endif  //__XRTCSERVER_ICE_ICE_STUN_H_


