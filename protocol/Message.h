#pragma once

// 채팅 메시지의 정의와 전송 포맷.
// 직렬화와 파싱이 한 파일에 마주 앉아 있어야 포맷이 어긋나지 않는다.
//
//   [uint32 count]
//   메시지마다:
//     [int64  id]
//     [int64  sender_id]
//     [int64  created_epoch]
//     [uint32 nick_len]   [nick 바이트]
//     [uint32 content_len][content 바이트]

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

#include "Type.h"
#include "Codec.h"

namespace msg {

struct Message {
    MessageId   id = 0;
    UserId      sender_id = 0;
    int64_t     created_epoch = 0;      // 1970-01-01 UTC 기준 초
    std::string sender_nick;
    std::string content;
};

inline std::vector<std::byte> serialize(const std::vector<Message>& in) {
    std::vector<std::byte> out;
    out.reserve(in.size() * 128);       // 대략적 예약

    uint32_t count = static_cast<uint32_t>(in.size());
    codec::put(out, &count, sizeof(count));

    for (const Message& m : in) {
        codec::put(out, &m.id, sizeof(m.id));
        codec::put(out, &m.sender_id, sizeof(m.sender_id));
        codec::put(out, &m.created_epoch, sizeof(m.created_epoch));
        codec::put_str(out, m.sender_nick);
        codec::put_str(out, m.content);
    }
    return out;
}

inline bool parse(const std::vector<std::byte>& body, std::vector<Message>& out) {
    codec::Reader r{body};

    uint32_t count = 0;
    if (!r.take(&count, sizeof(count))) return false;

    out.clear();
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        Message m;
        if (!r.take(&m.id, sizeof(m.id)))                       return false;
        if (!r.take(&m.sender_id, sizeof(m.sender_id)))         return false;
        if (!r.take(&m.created_epoch, sizeof(m.created_epoch))) return false;
        if (!r.take_str(m.sender_nick))                         return false;
        if (!r.take_str(m.content))                             return false;
        out.push_back(std::move(m));
    }
    return true;
}

}
