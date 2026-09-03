#pragma once

// 서버가 과거 메시지 응답 body 에 싣는 포맷을 해석한다.
// 서버측 db::message::serialize() 와 반드시 같은 규칙을 유지해야 한다.
//
//   [uint32 count]
//   메시지마다:
//     [int64  id]
//     [int64  sender_id]
//     [int64  created_epoch]
//     [uint32 nick_len]   [nick 바이트]
//     [uint32 content_len][content 바이트]

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace MessageCodec {

struct Message {
    int64_t     id = 0;
    int64_t     sender_id = 0;
    int64_t     created_epoch = 0;
    std::string sender_nick;
    std::string content;
};

// 길이를 넘어서는 읽기를 하지 않도록 항상 경계를 검사한다.
class Reader {
public:
    Reader(const char* p, size_t len) : p_(p), len_(len) {}

    bool take(void* dst, size_t n) {
        if (off_ + n > len_) return false;
        std::memcpy(dst, p_ + off_, n);
        off_ += n;
        return true;
    }
    bool take_str(std::string& s) {
        uint32_t l = 0;
        if (!take(&l, sizeof(l))) return false;
        if (off_ + l > len_) return false;
        s.assign(p_ + off_, l);
        off_ += l;
        return true;
    }
private:
    const char* p_;
    size_t len_;
    size_t off_ = 0;
};

inline bool parse(const std::string& body, std::vector<Message>& out) {
    Reader r(body.data(), body.size());

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

} // namespace MessageCodec
