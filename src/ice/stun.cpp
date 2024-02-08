

#include <rtc_base/logging.h>
#include <rtc_base/byte_order.h>
#include <rtc_base/crc32.h>
#include <rtc_base/message_digest.h>

#include "ice/stun.h"

namespace xrtc {

const char EMPTY_TRANSACION_ID[] = "000000000000";
const size_t STUN_FINGERPRINT_XOR_VALUE = 0x5354554e;
const char STUN_ERROR_REASON_BAD_REQUEST[] = "Bad request";
const char STUN_ERROR_REASON_UNAUTHORIZED[] = "Unauthorized";
const char STUN_ERROR_REASON_SERVER_ERROR[] = "Server error";

std::string stun_method_to_string(int type) {
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
    _type(0),
    _length(0),
    _transaction_id(EMPTY_TRANSACION_ID)
{
}

StunMessage::~StunMessage() = default;

bool StunMessage::validate_fingerprint(const char* data, size_t len) {
    // 检查长度
    size_t fingerprint_attr_size = k_stun_attribute_header_size +
        StunUInt32Attribute::SIZE;
    if (len % 4 != 0 || len < k_stun_header_size + fingerprint_attr_size) {
        return false;
    }
    
    // 检查magic cookie
    const char* magic_cookie = data + k_stun_transaction_id_offset -
        k_stun_magic_cookie_length;
    if (rtc::GetBE32(magic_cookie) != k_stun_magic_cookie) {
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
            k_stun_attribute_header_size);

    return (fingerprint ^ STUN_FINGERPRINT_XOR_VALUE) ==
        rtc::ComputeCrc32(data, len - fingerprint_attr_size);
}

StunMessage::IntegrityStatus StunMessage::validate_message_integrity(
        const std::string& password) 
{
    _password = password;
    if (get_byte_string(STUN_ATTR_MESSAGE_INTEGRITY)) {
        if (_validate_message_integrity_of_type(STUN_ATTR_MESSAGE_INTEGRITY,
                    k_stun_message_integrity_size,
                    _buffer.c_str(), _buffer.length(),
                    password))
        {
            _integrity = IntegrityStatus::k_integrity_ok;
        } else {
            _integrity = IntegrityStatus::k_integrity_bad;
        }
    } else {
        _integrity = IntegrityStatus::k_no_integrity;
    }

    return _integrity;
}

bool StunMessage::_validate_message_integrity_of_type(uint16_t mi_attr_type,
        size_t mi_attr_size, const char* data, size_t size,
        const std::string& password)
{
    if (size % 4 != 0 || size < k_stun_header_size) {
        return false;
    }
    
    uint16_t length = rtc::GetBE16(&data[2]);
    if (length + k_stun_header_size != size) {
        return false;
    }
    
    // 查找MI属性的位置
    size_t current_pos = k_stun_header_size;
    bool has_message_integrity = false;
    while (current_pos + k_stun_attribute_header_size <= size) {
        uint16_t attr_type;
        uint16_t attr_length;
        attr_type = rtc::GetBE16(&data[current_pos]);
        attr_length = rtc::GetBE16(&data[current_pos + sizeof(attr_type)]);
        if (attr_type == mi_attr_type) {
            has_message_integrity = true;
            break;
        }

        current_pos += (k_stun_attribute_header_size + attr_length);
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
    if (size > current_pos + k_stun_attribute_header_size + mi_attr_size) {
        size_t extra_pos = mi_pos + k_stun_attribute_header_size + mi_attr_size;
        size_t extra_size = size - extra_pos;
        size_t adjust_new_len = size - extra_size - k_stun_header_size;
        rtc::SetBE16(temp_data.get() + 2, adjust_new_len);
    }
    
    char hmac[k_stun_message_integrity_size];
    size_t ret = rtc::ComputeHmac(rtc::DIGEST_SHA_1, password.c_str(),
            password.length(), temp_data.get(), current_pos, hmac,
            sizeof(hmac));
    
    if (ret != k_stun_message_integrity_size) {
        return false;
    }

    return memcmp(data + mi_pos + k_stun_attribute_header_size, hmac, mi_attr_size)
        == 0;
}

bool StunMessage::add_message_integrity(const std::string& password) {
    return _add_message_integrity_of_type(STUN_ATTR_MESSAGE_INTEGRITY,
            k_stun_message_integrity_size, password.c_str(),
            password.size());
}

bool StunMessage::_add_message_integrity_of_type(uint16_t attr_type,
        uint16_t attr_size, const char* key, size_t key_len)
{
    auto mi_attr_ptr = std::make_unique<StunByteStringAttribute>(attr_type,
            std::string(attr_size, '0'));
    auto mi_attr = mi_attr_ptr.get();
    add_attribute(std::move(mi_attr_ptr));
    
    rtc::ByteBufferWriter buf;
    if (!write(&buf)) {
        return false;
    }
    
    size_t msg_len_for_hmac = buf.Length() - k_stun_attribute_header_size -
        mi_attr->length();
    char hmac[k_stun_message_integrity_size];
    size_t ret = rtc::ComputeHmac(rtc::DIGEST_SHA_1, key, key_len,
            buf.Data(), msg_len_for_hmac, hmac, sizeof(hmac));
    if (ret != sizeof(hmac)) {
        RTC_LOG(LS_WARNING) << "compute hmac error";
        return false;
    }
    
    mi_attr->copy_bytes(hmac, k_stun_message_integrity_size);
    _password.assign(key, key_len);
    _integrity = IntegrityStatus::k_integrity_ok;

    return true;
}

bool StunMessage::add_fingerprint() {
    auto fingerprint_attr_ptr = std::make_unique<StunUInt32Attribute>(
            STUN_ATTR_FINGERPRINT, 0);
    auto fingerprint_attr = fingerprint_attr_ptr.get();
    add_attribute(std::move(fingerprint_attr_ptr));
    
    rtc::ByteBufferWriter buf;
    if (!write(&buf)) {
        return false;
    }
    
    size_t msg_len_for_crc32 = buf.Length() - k_stun_attribute_header_size -
        fingerprint_attr->length();
    uint32_t c = rtc::ComputeCrc32(buf.Data(), msg_len_for_crc32);
    fingerprint_attr->set_value(c ^ STUN_FINGERPRINT_XOR_VALUE);
    return true;
}

void StunMessage::add_attribute(std::unique_ptr<StunAttribute> attr) {
    size_t attr_len = attr->length();
    if (attr_len % 4 != 0) {
        attr_len += (4 - (attr_len % 4));
    }

    _length += (attr_len + k_stun_attribute_header_size);

    _attrs.push_back(std::move(attr));
}

bool StunMessage::read(rtc::ByteBufferReader* buf) {
    if (!buf) {
        return false;
    }
    
    _buffer.assign(buf->Data(), buf->Length());

    if (!buf->ReadUInt16(&_type)) {
        return false;
    }
    
    // rtp/rtcp 10(2)
    if (_type & 0x8000) {
        return false;
    }
    
    if (!buf->ReadUInt16(&_length)) {
        return false;
    }
    
    std::string magic_cookie;
    if (!buf->ReadString(&magic_cookie, k_stun_magic_cookie_length)) {
        return false;
    }
    
    std::string transaction_id;
    if (!buf->ReadString(&transaction_id, k_stun_transaction_id_length)) {
        return false;
    }
    
    uint32_t magic_cookie_int;
    memcpy(&magic_cookie_int, magic_cookie.data(), sizeof(magic_cookie_int));
    if (rtc::NetworkToHost32(magic_cookie_int) != k_stun_magic_cookie) {
        transaction_id.insert(0, magic_cookie);
    }
    
    _transaction_id = transaction_id;
    
    if (buf->Length() != _length) {
        return false;
    }
    
    _attrs.resize(0);

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
                _create_attribute(attr_type, attr_length));
        if (!attr) {
            if (attr_length % 4 != 0) {
                attr_length += (4 - (attr_length % 4));
            }

            if (!buf->Consume(attr_length)) {
                return false;
            }
        } else {
            if (!attr->read(buf)) {
                return false;
            }

            _attrs.push_back(std::move(attr));
        }
    }

    return true;
}

bool StunMessage::write(rtc::ByteBufferWriter* buf) const {
    if (!buf) {
        return false;
    }

    buf->WriteUInt16(_type);
    buf->WriteUInt16(_length);
    buf->WriteUInt32(k_stun_magic_cookie);
    buf->WriteString(_transaction_id);
    
    for (const auto& attr : _attrs) {
        buf->WriteUInt16(attr->type());
        buf->WriteUInt16(attr->length());
        if (!attr->write(buf)) {
            return false;
        }
    }

    return true;
}

StunAttributeValueType StunMessage::get_attribute_value_type(int type) {
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

StunAttribute* StunMessage::_create_attribute(uint16_t type, 
        uint16_t length) 
{
    StunAttributeValueType value_type = get_attribute_value_type(type);
    if (STUN_VALUE_UNKNOWN != value_type) {
        return StunAttribute::create(value_type, type, length, this);
    }
    return nullptr;
}

const StunUInt32Attribute* StunMessage::get_uint32(uint16_t type) {
    return static_cast<const StunUInt32Attribute*>(_get_attribute(type));
}

const StunByteStringAttribute* StunMessage::get_byte_string(uint16_t type) {
    return static_cast<const StunByteStringAttribute*>(_get_attribute(type));
}

const StunErrorCodeAttribute* StunMessage::get_error_code() {
    return static_cast<const StunErrorCodeAttribute*>(
            _get_attribute(STUN_ATTR_ERROR_CODE));
}
    
int StunMessage::get_error_code_value() {
    auto error_attr = get_error_code();
    return error_attr ? error_attr->code() : STUN_ERROR_GLOBAL_FAIL;
}

const StunAttribute* StunMessage::_get_attribute(uint16_t type) {
    for (const auto& attr : _attrs) {
        if (attr->type() == type) {
            return attr.get();
        }
    }

    return nullptr;
}

StunAttribute::StunAttribute(uint16_t type, uint16_t length) :
    _type(type), _length(length) {}

StunAttribute::~StunAttribute() = default;

StunAttribute* StunAttribute::create(StunAttributeValueType value_type,
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

std::unique_ptr<StunErrorCodeAttribute> StunAttribute::create_error_code() {
    return std::make_unique<StunErrorCodeAttribute>(
            STUN_ATTR_ERROR_CODE, StunErrorCodeAttribute::MIN_SIZE);
}

void StunAttribute::consume_padding(rtc::ByteBufferReader* buf) {
    int remain = length() % 4;
    if (remain > 0) {
        buf->Consume(4 - remain);
    }
}

void StunAttribute::write_padding(rtc::ByteBufferWriter* buf) {
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
    _address = addr;
    
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
    switch (_address.family()) {
        case AF_INET:
            return STUN_ADDRESS_IPV4;
        case AF_INET6:
            return STUN_ADDRESS_IPV6;
        default:
            return STUN_ADDRESS_UNDEF;
    }
}

bool StunAddressAttribute::read(rtc::ByteBufferReader* /*buf*/) {
    return true;
}

bool StunAddressAttribute::write(rtc::ByteBufferWriter* buf) {
    StunAddressFamily stun_family = family();
    if (STUN_ADDRESS_UNDEF == stun_family) {
        RTC_LOG(LS_WARNING) << "write address attribute error: unknown family";
        return false;
    }

    buf->WriteUInt8(0);
    buf->WriteUInt8(stun_family);
    buf->WriteUInt16(_address.port());

    switch (_address.family()) {
        case AF_INET: {
            in_addr v4addr = _address.ipaddr().ipv4_address();
            buf->WriteBytes((const char*)&v4addr, sizeof(v4addr));
            break;
        }
        case AF_INET6: {
            in6_addr v6addr = _address.ipaddr().ipv6_address();
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

bool StunXorAddressAttribute::write(rtc::ByteBufferWriter* buf) {
    StunAddressFamily stun_family = family();
    if (STUN_ADDRESS_UNDEF == stun_family) {
        RTC_LOG(LS_WARNING) << "write address attribute error: unknown family";
        return false;
    }
    
    rtc::IPAddress xored_ip = _get_xored_ip();
    if (AF_UNSPEC == xored_ip.family()) {
        return false;
    }

    buf->WriteUInt8(0);
    buf->WriteUInt8(stun_family);
    buf->WriteUInt16(_address.port() ^ (k_stun_magic_cookie >> 16));

    switch (_address.family()) {
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

rtc::IPAddress StunXorAddressAttribute::_get_xored_ip() {
    rtc::IPAddress ip = _address.ipaddr();
    switch (_address.family()) {
        case AF_INET: {
            in_addr v4addr = ip.ipv4_address();
            v4addr.s_addr = (v4addr.s_addr ^ rtc::HostToNetwork32(k_stun_magic_cookie));
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
    StunAttribute(type, SIZE), _bits(0) {}

StunUInt32Attribute::StunUInt32Attribute(uint16_t type, uint32_t value) :
    StunAttribute(type, SIZE), _bits(value) {}

bool StunUInt32Attribute::read(rtc::ByteBufferReader* buf) {
    if (length() != SIZE || !buf->ReadUInt32(&_bits)) {
        return false;
    }
    return true;
}

bool StunUInt32Attribute::write(rtc::ByteBufferWriter* buf) {
    buf->WriteUInt32(_bits);
    return true;
}

// UInt64
StunUInt64Attribute::StunUInt64Attribute(uint16_t type) :
    StunAttribute(type, SIZE), _bits(0) {}

StunUInt64Attribute::StunUInt64Attribute(uint16_t type, uint64_t value) :
    StunAttribute(type, SIZE), _bits(value) {}

bool StunUInt64Attribute::read(rtc::ByteBufferReader* buf) {
    if (length() != SIZE || !buf->ReadUInt64(&_bits)) {
        return false;
    }
    return true;
}

bool StunUInt64Attribute::write(rtc::ByteBufferWriter* buf) {
    buf->WriteUInt64(_bits);
    return true;
}

// ByteString
StunByteStringAttribute::StunByteStringAttribute(uint16_t type, uint16_t length) :
    StunAttribute(type, length) {}
    
StunByteStringAttribute::StunByteStringAttribute(uint16_t type, const std::string& str) :
    StunAttribute(type, 0)
{
    copy_bytes(str.c_str(), str.size());
}

void StunByteStringAttribute::copy_bytes(const char* bytes, size_t len) {
    char* new_bytes = new char[len];
    memcpy(new_bytes, bytes, len);
    _set_bytes(new_bytes);
    set_length(len);
}

void StunByteStringAttribute::_set_bytes(char* bytes) {
    if (_bytes) {
        delete[] _bytes;
    }
    _bytes = bytes;
}

StunByteStringAttribute::~StunByteStringAttribute() {
    if (_bytes) {
        delete[] _bytes;
        _bytes = nullptr;
    }
}
    
bool StunByteStringAttribute::read(rtc::ByteBufferReader* buf) {
    _bytes = new char[length()];
    if (!buf->ReadBytes(_bytes, length())) {
        return false;
    }

    consume_padding(buf);

    return true;
}

bool StunByteStringAttribute::write(rtc::ByteBufferWriter* buf) {
    buf->WriteBytes(_bytes, length());
    write_padding(buf);
    return true;
}

// error code

const uint16_t StunErrorCodeAttribute::MIN_SIZE = 4;

StunErrorCodeAttribute::StunErrorCodeAttribute(uint16_t type, uint16_t length) :
    StunAttribute(type, length), _class(0), _number(0) {}

void StunErrorCodeAttribute::set_code(int code) {
    _class = code / 100;
    _number = code % 100;
}

int StunErrorCodeAttribute::code() const {
    return _class * 100 + _number;
}

void StunErrorCodeAttribute::set_reason(const std::string& reason) {
    _reason = reason;
    set_length(MIN_SIZE + reason.size());
}

bool StunErrorCodeAttribute::read(rtc::ByteBufferReader* /*buf*/) {
    // todo
    return false;
}

bool StunErrorCodeAttribute::write(rtc::ByteBufferWriter* buf) {
    buf->WriteUInt32(_class << 8 | _number);
    buf->WriteString(_reason);
    write_padding(buf);
    return true;
}

int get_stun_success_response(int req_type) {
    return is_stun_request_type(req_type) ? (req_type | 0x100) : -1;
}

int get_stun_error_response(int req_type) {
    return is_stun_request_type(req_type) ? (req_type | 0x110) : -1;
}

bool is_stun_request_type(int req_type) {
    return (req_type & k_stun_type_mask) == 0x000;
}

} // namespace xrtc


