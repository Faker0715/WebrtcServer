#include "ice/stun.h"

#include <rtc_base/logging.h>
#include <rtc_base/byte_order.h>
#include <rtc_base/crc32.h>
#include <rtc_base/message_digest.h>

namespace xrtc {

const char EMPTY_TRANSACION_ID[] = "000000000000";
const size_t STUN_FINGERPRINT_XOR_VALUE = 0x5354554e;
const char STUN_ERROR_REASON_BAD_REQUEST[] = "Bad request";
const char STUN_ERROR_REASON_UNAUTHORIZED[] = "Unauthorized";
const char STUN_ERROR_REASON_SERVER_ERROR[] = "Server error";

std::string StunMethodToString(int type) {
    switch (type) {
        case STUN_BINDING_REQUEST:
            return "BINDING REQUEST";
        case STUN_BINDING_RESPONSE:
            return "BINDING RESPONSE";
        case STUN_BINDING_ERROR_RESPONSE:
            return "BINDING ERROR_RESPONSE";
        default:
            return "Unknown<" + std::to_string(type) + ">";
    }
}

StunMessage::StunMessage() :
    type_(0),
    length_(0),
    transaction_id_(EMPTY_TRANSACION_ID)
{
}

StunMessage::~StunMessage() = default;

bool StunMessage::ValidateFingerprint(const char* data, size_t len) {
    // 检查长度
    size_t fingerprint_attr_size = kStunAttributeHeaderSize +
        StunUInt32Attribute::SIZE;
    if (len % 4 != 0 || len < kStunHeaderSize + fingerprint_attr_size) {
        return false;
    }
    
    // 检查magic cookie
    const char* magic_cookie = data + kStunTransactionIdOffset -
        kStunMagicCookieLength;
    if (rtc::GetBE32(magic_cookie) != kStunMagicCookie) {
        return false;
    }
    
    // 检查attr type和length
    const char* fingerprint_attr_data = data + len - fingerprint_attr_size;
    if (rtc::GetBE16(fingerprint_attr_data) != STUN_ATTR_FINGERPRINT ||
            rtc::GetBE16(fingerprint_attr_data + sizeof(uint16_t)) !=
            StunUInt32Attribute::SIZE) 
    {
        return false;
    }
    
    // 检查fingerprint的值
    uint32_t fingerprint = rtc::GetBE32(fingerprint_attr_data + 
            kStunAttributeHeaderSize);

    return (fingerprint ^ STUN_FINGERPRINT_XOR_VALUE) ==
        rtc::ComputeCrc32(data, len - fingerprint_attr_size);
}

StunMessage::IntegrityStatus StunMessage::ValidateMessageIntegrity(
        const std::string& password) 
{
    password_ = password;
    if (GetByteString(STUN_ATTR_MESSAGE_INTEGRITY)) {
        if (ValidateMessageIntegrityOfType(STUN_ATTR_MESSAGE_INTEGRITY,
                    kStunMessageIntegritySize,
                    buffer_.c_str(), buffer_.length(),
                    password))
        {
            integrity_ = IntegrityStatus::kIntegrityOk;
        } else {
            integrity_ = IntegrityStatus::kIntegrityBad;
        }
    } else {
        integrity_ = IntegrityStatus::kNoIntegrity;
    }

    return integrity_;
}

bool StunMessage::ValidateMessageIntegrityOfType(uint16_t mi_attr_type,
        size_t mi_attr_size, const char* data, size_t size,
        const std::string& password)
{
    if (size % 4 != 0 || size < kStunHeaderSize) {
        return false;
    }
    
    uint16_t length = rtc::GetBE16(&data[2]);
    if (length + kStunHeaderSize != size) {
        return false;
    }
    
    // 查找MI属性的位置
    size_t current_pos = kStunHeaderSize;
    bool has_message_integrity = false;
    while (current_pos + kStunAttributeHeaderSize <= size) {
        uint16_t attr_type;
        uint16_t attr_length;
        attr_type = rtc::GetBE16(&data[current_pos]);
        attr_length = rtc::GetBE16(&data[current_pos + sizeof(attr_type)]);
        if (attr_type == mi_attr_type) {
            has_message_integrity = true;
            break;
        }

        current_pos += (kStunAttributeHeaderSize + attr_length);
        if (attr_length % 4 != 0) {
            current_pos += (4 - (attr_length % 4));
        }
    }
    
    if (!has_message_integrity) {
        return false;
    }
    
    size_t mi_pos = current_pos;
    std::unique_ptr<char[]> temp_data(new char[mi_pos]);
    memcpy(temp_data.get(), data, mi_pos); 
    if (size > current_pos + kStunAttributeHeaderSize + mi_attr_size) {
        size_t extra_pos = mi_pos + kStunAttributeHeaderSize + mi_attr_size;
        size_t extra_size = size - extra_pos;
        size_t adjust_new_len = size - extra_size - kStunHeaderSize;
        rtc::SetBE16(temp_data.get() + 2, adjust_new_len);
    }
    
    char hmac[kStunMessageIntegritySize];
    size_t ret = rtc::ComputeHmac(rtc::DIGEST_SHA_1, password.c_str(),
            password.length(), temp_data.get(), current_pos, hmac,
            sizeof(hmac));
    
    if (ret != kStunMessageIntegritySize) {
        return false;
    }

    return memcmp(data + mi_pos + kStunAttributeHeaderSize, hmac, mi_attr_size)
        == 0;
}

bool StunMessage::AddMessageIntegrity(const std::string& password) {
    return AddMessageIntegrityOfType(STUN_ATTR_MESSAGE_INTEGRITY,
            kStunMessageIntegritySize, password.c_str(),
            password.size());
}

bool StunMessage::AddMessageIntegrityOfType(uint16_t attr_type,
        uint16_t attr_size, const char* key, size_t key_len)
{
    auto mi_attr_ptr = std::make_unique<StunByteStringAttribute>(attr_type,
            std::string(attr_size, '0'));
    auto mi_attr = mi_attr_ptr.get();
    AddAttribute(std::move(mi_attr_ptr));
    
    rtc::ByteBufferWriter buf;
    if (!Write(&buf)) {
        return false;
    }
    
    size_t msg_len_for_hmac = buf.Length() - kStunAttributeHeaderSize -
        mi_attr->length();
    char hmac[kStunMessageIntegritySize];
    size_t ret = rtc::ComputeHmac(rtc::DIGEST_SHA_1, key, key_len,
            buf.Data(), msg_len_for_hmac, hmac, sizeof(hmac));
    if (ret != sizeof(hmac)) {
        RTC_LOG(LS_WARNING) << "compute hmac error";
        return false;
    }
    
    mi_attr->CopyBytes(hmac, kStunMessageIntegritySize);
    password_.assign(key, key_len);
    integrity_ = IntegrityStatus::kIntegrityOk;

    return true;
}

bool StunMessage::AddFingerprint() {
    auto fingerprint_attr_ptr = std::make_unique<StunUInt32Attribute>(
            STUN_ATTR_FINGERPRINT, 0);
    auto fingerprint_attr = fingerprint_attr_ptr.get();
    AddAttribute(std::move(fingerprint_attr_ptr));
    
    rtc::ByteBufferWriter buf;
    if (!Write(&buf)) {
        return false;
    }
    
    size_t msg_len_for_crc32 = buf.Length() - kStunAttributeHeaderSize -
        fingerprint_attr->length();
    uint32_t c = rtc::ComputeCrc32(buf.Data(), msg_len_for_crc32);
    fingerprint_attr->set_value(c ^ STUN_FINGERPRINT_XOR_VALUE);
    return true;
}

void StunMessage::AddAttribute(std::unique_ptr<StunAttribute> attr) {
    size_t attr_len = attr->length();
    if (attr_len % 4 != 0) {
        attr_len += (4 - (attr_len % 4));
    }

    length_ += (attr_len + kStunAttributeHeaderSize);

    attrs_.push_back(std::move(attr));
}

bool StunMessage::Read(rtc::ByteBufferReader* buf) {
    if (!buf) {
        return false;
    }
    
    buffer_.assign(buf->Data(), buf->Length());

    if (!buf->ReadUInt16(&type_)) {
        return false;
    }
    
    // rtp/rtcp 10(2)
    if (type_ & 0x8000) {
        return false;
    }
    
    if (!buf->ReadUInt16(&length_)) {
        return false;
    }
    
    std::string magic_cookie;
    if (!buf->ReadString(&magic_cookie, kStunMagicCookieLength)) {
        return false;
    }
    
    std::string transaction_id;
    if (!buf->ReadString(&transaction_id, kStunTransactionIdLength)) {
        return false;
    }
    
    uint32_t magic_cookie_int;
    memcpy(&magic_cookie_int, magic_cookie.data(), sizeof(magic_cookie_int));
    if (rtc::NetworkToHost32(magic_cookie_int) != kStunMagicCookie) {
        transaction_id.insert(0, magic_cookie);
    }
    
    transaction_id_ = transaction_id;
    
    if (buf->Length() != length_) {
        return false;
    }
    
    attrs_.resize(0);

    while (buf->Length() > 0) {
        uint16_t attr_type;
        uint16_t attr_length;
        if (!buf->ReadUInt16(&attr_type)) {
            return false;
        }

        if (!buf->ReadUInt16(&attr_length)) {
            return false;
        }
        
        std::unique_ptr<StunAttribute> attr(
                CreateAttribute(attr_type, attr_length));
        if (!attr) {
            if (attr_length % 4 != 0) {
                attr_length += (4 - (attr_length % 4));
            }

            if (!buf->Consume(attr_length)) {
                return false;
            }
        } else {
            if (!attr->Read(buf)) {
                return false;
            }

            attrs_.push_back(std::move(attr));
        }
    }

    return true;
}

bool StunMessage::Write(rtc::ByteBufferWriter* buf) const {
    if (!buf) {
        return false;
    }

    buf->WriteUInt16(type_);
    buf->WriteUInt16(length_);
    buf->WriteUInt32(kStunMagicCookie);
    buf->WriteString(transaction_id_);
    
    for (const auto& attr : attrs_) {
        buf->WriteUInt16(attr->type());
        buf->WriteUInt16(attr->length());
        if (!attr->Write(buf)) {
            return false;
        }
    }

    return true;
}

StunAttributeValueType StunMessage::GetAttributeValueType(int type) {
    switch (type) {
        case STUN_ATTR_USERNAME:
            return STUN_VALUE_BYTE_STRING;
        case STUN_ATTR_MESSAGE_INTEGRITY:
            return STUN_VALUE_BYTE_STRING;
        case STUN_ATTR_PRIORITY:
            return STUN_VALUE_UINT32;
        default:
            return STUN_VALUE_UNKNOWN;
    }
}

StunAttribute* StunMessage::CreateAttribute(uint16_t type, 
        uint16_t length) 
{
    StunAttributeValueType value_type = GetAttributeValueType(type);
    if (STUN_VALUE_UNKNOWN != value_type) {
        return StunAttribute::Create(value_type, type, length, this);
    }
    return nullptr;
}

const StunUInt32Attribute* StunMessage::GetUInt32(uint16_t type) {
    return static_cast<const StunUInt32Attribute*>(GetAttribute(type));
}

const StunByteStringAttribute* StunMessage::GetByteString(uint16_t type) {
    return static_cast<const StunByteStringAttribute*>(GetAttribute(type));
}

const StunErrorCodeAttribute* StunMessage::GetErrorCode() {
    return static_cast<const StunErrorCodeAttribute*>(
            GetAttribute(STUN_ATTR_ERROR_CODE));
}
    
int StunMessage::GetErrorCodeValue() {
    auto error_attr = GetErrorCode();
    return error_attr ? error_attr->code() : STUN_ERROR_GLOBAL_FAIL;
}

const StunAttribute* StunMessage::GetAttribute(uint16_t type) {
    for (const auto& attr : attrs_) {
        if (attr->type() == type) {
            return attr.get();
        }
    }

    return nullptr;
}

StunAttribute::StunAttribute(uint16_t type, uint16_t length) :
    type_(type), length_(length) {}

StunAttribute::~StunAttribute() = default;

StunAttribute* StunAttribute::Create(StunAttributeValueType value_type,
        uint16_t type, uint16_t length, void* /*owner*/)
{
    switch (value_type) {
        case STUN_VALUE_BYTE_STRING:
            return new StunByteStringAttribute(type, length);
        case STUN_VALUE_UINT32:
            return new StunUInt32Attribute(type);
        default:
            return nullptr;
    }
}

std::unique_ptr<StunErrorCodeAttribute> StunAttribute::CreateErrorCode() {
    return std::make_unique<StunErrorCodeAttribute>(
            STUN_ATTR_ERROR_CODE, StunErrorCodeAttribute::MIN_SIZE);
}

void StunAttribute::ConsumePadding(rtc::ByteBufferReader* buf) {
    int remain = length() % 4;
    if (remain > 0) {
        buf->Consume(4 - remain);
    }
}

void StunAttribute::WritePadding(rtc::ByteBufferWriter* buf) {
    int remain = length() % 4;
    if (remain > 0) {
        char zeroes[4] = {0};
        buf->WriteBytes(zeroes, 4 - remain);
    }
}

// Address
StunAddressAttribute::StunAddressAttribute(uint16_t type, 
        const rtc::SocketAddress& addr) :
    StunAttribute(type, 0)
{
    set_address(addr);
}

void StunAddressAttribute::set_address(const rtc::SocketAddress& addr) {
    address_ = addr;
    
    switch (family()) {
        case STUN_ADDRESS_IPV4:
            set_length(SIZE_IPV4);
            break;
        case STUN_ADDRESS_IPV6:
            set_length(SIZE_IPV6);
            break;
        default:
            set_length(SIZE_UNDEF);
            break;
    }
}

StunAddressFamily StunAddressAttribute::family() {
    switch (address_.family()) {
        case AF_INET:
            return STUN_ADDRESS_IPV4;
        case AF_INET6:
            return STUN_ADDRESS_IPV6;
        default:
            return STUN_ADDRESS_UNDEF;
    }
}

bool StunAddressAttribute::Read(rtc::ByteBufferReader* /*buf*/) {
    return true;
}

bool StunAddressAttribute::Write(rtc::ByteBufferWriter* buf) {
    StunAddressFamily stun_family = family();
    if (STUN_ADDRESS_UNDEF == stun_family) {
        RTC_LOG(LS_WARNING) << "write address attribute error: unknown family";
        return false;
    }

    buf->WriteUInt8(0);
    buf->WriteUInt8(stun_family);
    buf->WriteUInt16(address_.port());

    switch (address_.family()) {
        case AF_INET: {
            in_addr v4addr = address_.ipaddr().ipv4_address();
            buf->WriteBytes((const char*)&v4addr, sizeof(v4addr));
            break;
        }
        case AF_INET6: {
            in6_addr v6addr = address_.ipaddr().ipv6_address();
            buf->WriteBytes((const char*)&v6addr, sizeof(v6addr));
            break;
        }
        default:
            return false;
    }

    return true;
}

// Xor Address
StunXorAddressAttribute::StunXorAddressAttribute(uint16_t type, 
        const rtc::SocketAddress& addr) :
    StunAddressAttribute(type, addr)
{
}

bool StunXorAddressAttribute::Write(rtc::ByteBufferWriter* buf) {
    StunAddressFamily stun_family = family();
    if (STUN_ADDRESS_UNDEF == stun_family) {
        RTC_LOG(LS_WARNING) << "write address attribute error: unknown family";
        return false;
    }
    
    rtc::IPAddress xored_ip = GetXoredIP();
    if (AF_UNSPEC == xored_ip.family()) {
        return false;
    }

    buf->WriteUInt8(0);
    buf->WriteUInt8(stun_family);
    buf->WriteUInt16(address_.port() ^ (kStunMagicCookie >> 16));

    switch (address_.family()) {
        case AF_INET: {
            in_addr v4addr = xored_ip.ipv4_address();
            buf->WriteBytes((const char*)&v4addr, sizeof(v4addr));
            break;
        }
        case AF_INET6: {
            in6_addr v6addr = xored_ip.ipv6_address();
            buf->WriteBytes((const char*)&v6addr, sizeof(v6addr));
            break;
        }
        default:
            return false;
    }

    return true;
}

rtc::IPAddress StunXorAddressAttribute::GetXoredIP() {
    rtc::IPAddress ip = address_.ipaddr();
    switch (address_.family()) {
        case AF_INET: {
            in_addr v4addr = ip.ipv4_address();
            v4addr.s_addr = (v4addr.s_addr ^ rtc::HostToNetwork32(kStunMagicCookie));
            return rtc::IPAddress(v4addr);
        }
        case AF_INET6:
            break;
        default:
            break;
    }
    return rtc::IPAddress();
}

// UInt32
StunUInt32Attribute::StunUInt32Attribute(uint16_t type) :
    StunAttribute(type, SIZE), bits_(0) {}

StunUInt32Attribute::StunUInt32Attribute(uint16_t type, uint32_t value) :
    StunAttribute(type, SIZE), bits_(value) {}

bool StunUInt32Attribute::Read(rtc::ByteBufferReader* buf) {
    if (length() != SIZE || !buf->ReadUInt32(&bits_)) {
        return false;
    }
    return true;
}

bool StunUInt32Attribute::Write(rtc::ByteBufferWriter* buf) {
    buf->WriteUInt32(bits_);
    return true;
}

// UInt64
StunUInt64Attribute::StunUInt64Attribute(uint16_t type) :
    StunAttribute(type, SIZE), bits_(0) {}

StunUInt64Attribute::StunUInt64Attribute(uint16_t type, uint64_t value) :
    StunAttribute(type, SIZE), bits_(value) {}

bool StunUInt64Attribute::Read(rtc::ByteBufferReader* buf) {
    if (length() != SIZE || !buf->ReadUInt64(&bits_)) {
        return false;
    }
    return true;
}

bool StunUInt64Attribute::Write(rtc::ByteBufferWriter* buf) {
    buf->WriteUInt64(bits_);
    return true;
}

// ByteString
StunByteStringAttribute::StunByteStringAttribute(uint16_t type, uint16_t length) :
    StunAttribute(type, length) {}
    
StunByteStringAttribute::StunByteStringAttribute(uint16_t type, const std::string& str) :
    StunAttribute(type, 0)
{
    CopyBytes(str.c_str(), str.size());
}

void StunByteStringAttribute::CopyBytes(const char* bytes, size_t len) {
    char* new_bytes = new char[len];
    memcpy(new_bytes, bytes, len);
    set_bytes(new_bytes);
    set_length(len);
}

void StunByteStringAttribute::set_bytes(char* bytes) {
    if (bytes_) {
        delete[] bytes_;
    }
    bytes_ = bytes;
}

StunByteStringAttribute::~StunByteStringAttribute() {
    if (bytes_) {
        delete[] bytes_;
        bytes_ = nullptr;
    }
}
    
bool StunByteStringAttribute::Read(rtc::ByteBufferReader* buf) {
    bytes_ = new char[length()];
    if (!buf->ReadBytes(bytes_, length())) {
        return false;
    }

    ConsumePadding(buf);

    return true;
}

bool StunByteStringAttribute::Write(rtc::ByteBufferWriter* buf) {
    buf->WriteBytes(bytes_, length());
    WritePadding(buf);
    return true;
}

// error code

const uint16_t StunErrorCodeAttribute::MIN_SIZE = 4;

StunErrorCodeAttribute::StunErrorCodeAttribute(uint16_t type, uint16_t length) :
    StunAttribute(type, length), class_(0), number_(0) {}

void StunErrorCodeAttribute::set_code(int code) {
    class_ = code / 100;
    number_ = code % 100;
}

int StunErrorCodeAttribute::code() const {
    return class_ * 100 + number_;
}

void StunErrorCodeAttribute::set_reason(const std::string& reason) {
    reason_ = reason;
    set_length(MIN_SIZE + reason.size());
}

bool StunErrorCodeAttribute::Read(rtc::ByteBufferReader* /*buf*/) {
    // todo
    return false;
}

bool StunErrorCodeAttribute::Write(rtc::ByteBufferWriter* buf) {
    buf->WriteUInt32(class_ << 8 | number_);
    buf->WriteString(reason_);
    WritePadding(buf);
    return true;
}

int GetStunSuccessResponse(int req_type) {
    return IsStunRequestType(req_type) ? (req_type | 0x100) : -1;
}

int GetStunErrorResponse(int req_type) {
    return IsStunRequestType(req_type) ? (req_type | 0x110) : -1;
}

bool IsStunRequestType(int req_type) {
    return (req_type & kStunTypeMask) == 0x000;
}

} // namespace xrtc


