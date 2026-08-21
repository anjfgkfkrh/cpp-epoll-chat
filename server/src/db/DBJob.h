#pragma once

#include <functional>
#include <vector>
#include <cstdint>
#include <string>
#include <cstddef>

enum class DBJobType : uint8_t {
    SaveMessage,
    LoadHistory,
    CreateRoom,
    FindAccount,
    CreateAccount,
};

enum class DBStatus : uint8_t {
    Success,    // 정상, data에 결과
    NotFound,   // 쿼리는 성공했으나 0행
    Duplicate,  // UNIQUE 위반 (23505)
    Error,      // 연결 끊김, 타임아웃 등
};

struct DBJob {
    DBJobType type;
    std::vector<std::string> params;
    std::function<void(DBStatus, std::vector<std::byte>&&)> on_result;
};