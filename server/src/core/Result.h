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
    DBNotFound,
    DBError,
    DBDuplicate,
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
    DBNotFound,
    DBError,
    DBDuplicate,
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
    case Result::DBNotFound:
        return ResponseResultCode::DBNotFound;
    case Result::DBError:
        return ResponseResultCode::DBError;
    case Result::DBDuplicate:
        return ResponseResultCode::DBDuplicate;
    default:
        return ResponseResultCode::None;
    }
}