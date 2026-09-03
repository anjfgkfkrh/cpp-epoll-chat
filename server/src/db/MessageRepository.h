#pragma once

#include "DBConnection.h"
#include "DBResult.h"
#include "Type.h"
#include "Message.h"

#include <vector>
#include <string>
#include <cstddef>

namespace db::message {

inline constexpr const char* kInsert = 
    "INSERT INTO messages(room_id, sender_id, sender_nick, content) "
    "VALUES($1, $2, $3, $4)";

inline constexpr const char* kSelectRecent = 
    "SELECT id, sender_id, sender_nick, content, "
    "   EXTRACT(EPOCH FROM created_at)::bigint AS created_epoch "
    "FROM messages WHERE room_id = $1 AND id < $2 "
    "ORDER BY id DESC LIMIT 50";

inline DBResult save(DBConnection& conn, const std::vector<std::string>& params) {
    return conn.exec(kInsert, params);
}
inline DBResult load_recent(DBConnection& conn, const std::vector<std::string>& params) {
    return conn.exec(kSelectRecent, params);
}

inline std::vector<std::string> params_for_save(RoomId room_id, UserId sender_id, std::string& sender_nick, std::string& content) {
    return {std::to_string(room_id), std::to_string(sender_id), sender_nick, content};
}

// 조회 결과를 도메인 구조체로 옮긴다. 컬럼명을 아는 것은 이 계층뿐이며,
// 바이트 변환은 msg::serialize() 가 맡는다.
inline std::vector<msg::Message> to_messages(const DBResult& res) {
    std::vector<msg::Message> out;
    out.reserve(res.rows());

    for (int i = 0; i < res.rows(); ++i) {
        msg::Message m;
        m.id            = res.get_int64(i, "id");
        m.sender_id     = res.get_int64(i, "sender_id");
        m.created_epoch = res.get_int64(i, "created_epoch");
        m.sender_nick   = res.get_string(i, "sender_nick");
        m.content       = res.get_string(i, "content");
        out.push_back(std::move(m));
    }
    return out;
}

}