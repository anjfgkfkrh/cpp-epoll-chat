#pragma once

#include "DBConnection.h"
#include "DBResult.h"

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

namespace db::account {
    
inline constexpr const char* kFindByLoginId = 
    "SELECT user_id, pass_hash, nickname "
    "FROM account WHERE login_id = $1";

inline constexpr const char* kInsert = 
    "INSERT INTO account(login_id, pass_hash, nickname) "
    "VALUES($1, $2, $3) RETURNING user_id";

inline DBResult find(DBConnection& conn, const std::vector<std::string>& params) {
    return conn.exec(kFindByLoginId, params);
}

inline DBResult create(DBConnection& conn, const std::vector<std::string>& params) {
    return conn.exec(kInsert, params);
}

inline std::vector<std::string> params_for_find(const std::string& login_id) {
    return { login_id };
}

inline std::vector<std::string> params_for_create(const std::string& login_id, const std::string& pass_hash, const std::string& nickname) {
    return { login_id, pass_hash, nickname };
}

struct Row {
    int64_t     user_id = 0;
    std::string pass_hash;
    std::string nickname;
};

std::vector<std::byte> serialize(const DBResult& res);
bool parse(const std::vector<std::byte>& data, Row& out);
}