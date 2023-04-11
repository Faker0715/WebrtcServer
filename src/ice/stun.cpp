//
// Created by faker on 23-4-11.
//

#include "stun.h"
#include "rtc_base/byte_order.h"
#include "rtc_base/crc32.h"

namespace xrtc {
    // 占用12个字节
    const char EMPTY_TRANSACTION_ID[] = "000000000000";
    const size_t STUN_FINGERPRINT_XOR_VALUE = 0x5354554e;

    StunMessage::StunMessage() : _type(0), _length(0), _transaction_id(EMPTY_TRANSACTION_ID) {

    }

    bool StunMessage::vaildate_fingerprint(const char *data, size_t len) {
        // 检查长度
        size_t finerprint_attr_size = k_stun_attribute_header_size + StunUInt32Attribute::SIZE;
        if (len % 4 != 0 || len < k_stun_header_size + finerprint_attr_size) {
            return false;
        }
        // 检查magic cookie
        const char *magic_cookie = data + k_stun_transaction_id_offset - k_stun_magic_cookie_length;
        if (rtc::GetBE32(magic_cookie) != k_stun_magic_cookie) {
            return false;
        }
        // 检查attr type 和 length
        const char *fingerprint_attr_data = data + len - finerprint_attr_size;
        if (rtc::GetBE16(fingerprint_attr_data) != STUN_ATTR_FINGERPRINT ||
            rtc::GetBE16(fingerprint_attr_data + sizeof(uint16_t)) != StunUInt32Attribute::SIZE)
            return false;

        // 检查fingerprint的值
        uint32_t fingerprint = rtc::GetBE32(fingerprint_attr_data + k_stun_attribute_header_size);

        return fingerprint ^ STUN_FINGERPRINT_XOR_VALUE == rtc::ComputeCrc32(data, len - finerprint_attr_size);

    }

    bool StunMessage::read(rtc::ByteBufferReader *buf) {
        if (!buf) {
            return false;
        }
        if (!buf->ReadUInt16(&_type)) {
            return false;
        }
        // rtc/rtcp 10
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

            std::unique_ptr<StunAttribute> attr = _create_attribute(attr_type, attr_length);

            if (!attr) {
                if (attr_length % 4 != 0) {
                    attr_length += (4 - (attr_length % 4));
                }
                if (!buf->Consume(attr_length)) {
                    return false;
                }
            } else {
                if (!(attr->read(buf))) {
                    return false;
                }
                _attrs.push_back(std::move(attr));
            }


        }

        return true;
    }

    std::unique_ptr<StunAttribute> StunMessage::_create_attribute(uint16_t type, uint16_t length) {
        return std::unique_ptr<StunAttribute>();
    }

    StunMessage::~StunMessage() = default;
}