#pragma once

#include <cstdint>

enum class ResponseResultCode : uint8_t {
    // 기본값
    None,
    
    // 성공 코드
    Success,

    // 실패 코드
    RoomNotFound,
    RoomAlreadyExists,
    RoomFull,
    UserNotFound,
};

enum class Result : uint8_t {
    // 기본값
    None,

    // 성공 코드
    Success,

    // 실패 코드
    RoomNotFound,
    RoomAlreadyExists,
    RoomFull,
    UserNotFound,
};

inline ResponseResultCode result_to_resresultcode(Result result) {
    switch(result)
    {
    case Result::Success:
        return ResponseResultCode::Success;
    case Result::RoomNotFound:
        return ResponseResultCode::RoomNotFound;
    case Result::RoomAlreadyExists:
        return ResponseResultCode::RoomAlreadyExists;
    case Result::RoomFull:
        return ResponseResultCode::RoomFull;
    case Result::UserNotFound:
        return ResponseResultCode::UserNotFound;
    default:
        return ResponseResultCode::None;
    }
}