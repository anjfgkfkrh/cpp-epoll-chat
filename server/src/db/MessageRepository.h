#pragma once

#include "DBConnection.h"
#include "DBResult.h"

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

struct Message {
    int64_t     id;
    int64_t     sender_id;
    int64_t     created_epoch;
    std::string sender_nick;
    std::string content;
};

std::vector<std::byte> serialize(const DBResult& res);
bool parse(const std::vector<std::byte>& data, std::vector<Message>& out);

}