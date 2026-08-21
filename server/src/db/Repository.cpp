#include "MessageRepository.h"
#include "AccountRepository.h"
#include "Codec.h"

#include <cstring>

namespace db::message {
    std::vector<std::byte> serialize(const DBResult& res){
        std::vector<std::byte> out;
        out.reserve(res.rows() * 128);      // 대략적 예약

        uint32_t count = static_cast<uint32_t>(res.rows());
        codec::put(out, &count, sizeof(count));

        for(int i=0; i<res.rows(); i++) {
            int64_t id = res.get_int64(i, "id");
            int64_t sender = res.get_int64(i, "sender_id");
            int64_t epoch = res.get_int64(i, "created_epoch");
            codec::put(out, &id, sizeof(id));
            codec::put(out, &sender, sizeof(sender));
            codec::put(out, &epoch, sizeof(epoch));
            codec::put_str(out, res.get_string(i, "sender_nick"));
            codec::put_str(out, res.get_string(i, "content"));
        }

        return out;
    }

    bool parse(const std::vector<std::byte>& data, std::vector<Message>& out) {
        codec::Reader r{ data.data(), data.size() };

        uint32_t count;
        if(!r.take(&count, sizeof(count))) return false;

        out.reserve(count);
        for(uint32_t i=0; i<count; ++i) {
            Message m;
            if(!r.take(&m.id, sizeof(m.id)))                  return false;
            if(!r.take(&m.sender_id, sizeof(m.sender_id)))    return false;
            if(!r.take(&m.created_epoch, sizeof(m.created_epoch)))    return false;
            if(!r.take_str(m.sender_nick))                    return false;
            if(!r.take_str(m.content))                        return false;
            out.push_back(std::move(m));
        }
        return true;
    }
}

namespace db::account {
    std::vector<std::byte> serialize(const DBResult& res) {
        std::vector<std::byte> out;
        if(res.rows() == 0) return out;

        int64_t user_id = res.get_int64(0, "user_id");
        codec::put(out, &user_id, sizeof(user_id));
        codec::put_str(out, res.get_string(0, "pass_hash"));
        codec::put_str(out, res.get_string(0, "nickname"));
        return out;
    }

    bool parse(const std::vector<std::byte>& data, Row& out) {
        codec::Reader r{ data.data(), data.size() };
        if(!r.take(&out.user_id, sizeof(out.user_id)))  return false;
        if(!r.take_str(out.pass_hash))                  return false;
        if(!r.take_str(out.nickname))                   return false;
        return true;
    }
}