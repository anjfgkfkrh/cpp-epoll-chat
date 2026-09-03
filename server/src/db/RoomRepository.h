#pragma once

#include "DBConnection.h"
#include "DBResult.h"
#include "Codec.h"
#include "Type.h"

#include <vector>
#include <string>


namespace db::room {

inline constexpr const char* kInsert = 
    "INSERT INTO room DEFAULT VALUES RETURNING room_id";

inline DBResult create(DBConnection& conn, const std::vector<std::string>& params) {
    return conn.exec(kInsert, params);
}

inline std::vector<std::byte> serialize(const DBResult& res) {
    std::vector<std::byte> out;
    if(res.rows() == 0)  return out;
    RoomId room_id = res.get_int64(0, "room_id");
    codec::put(out, &room_id, sizeof(room_id));
    return out;
}

}