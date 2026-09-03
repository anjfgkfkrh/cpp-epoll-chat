#pragma once

#include "Protocol.h"
#include "Result.h"
#include "EventState.h"
#include "Session.h"
#include "Type.h"

#include <memory>
#include <array>
#include <limits>

constexpr RoomId PENDING_ROOM_ID = std::numeric_limits<RoomId>::max();

enum class ActionCommand : uint8_t {
    CREATE_ROOM,
    JOIN_ROOM,
    LEAVE_ROOM,
    SEND_MESSAGE,

    REQUEST_DB_CREATE_ROOM,
    REQUEST_DB_LOAD_MESSAGE,
    SAVE_MESSAGE,
    
    REQUEST_JOIN_ROOM,
    REQUEST_LEAVE_ROOM,

    SEND_RESPONSE,
    FLUSH,  // epoll out 상황에서의 flush 요청

    DISCONNECT_SESSION,
};

struct RoomAction {
    ActionCommand command;
    RoomId target_room_id;
};

struct RoomEvent {
    std::array<RoomAction, 10> actions;     // 처리 해야할 행동 리스트
    uint8_t current_action = 0;             // 현재 action
    uint8_t action_len = 0;                 // action 갯수
    RoomId target_room_id = 0;            // 이벤트 요청 목표 Room ID

    Protocol::Command initial_command;      // 사용자가 보낸 최초 명령

    int fd = -1;                                // disconnect_session을 위한 변수
    std::shared_ptr<Session> session = nullptr; // Session
    std::vector<std::byte> request_data = {};           // body data
    std::vector<std::byte> response_data = {};

    Result result_code = Result::None;          // 처리 결과 코드

    bool add_action(ActionCommand cmd, RoomId room_id=0) {
        if(action_len >= actions.size())
            return false;

        actions[action_len++] = {cmd, room_id};

        return true;
    }

    void resolve_room_id(RoomId room_id) {
        for (uint8_t i=0; i<action_len; ++i)
            if(actions[i].target_room_id == PENDING_ROOM_ID)
                actions[i].target_room_id = room_id;
        target_room_id = room_id;
    }
};

inline ActionCommand to_event_command(Protocol::Command command) {
    switch(command) {
    case Protocol::Command::CMD_CREATE_ROOM:
        return ActionCommand::CREATE_ROOM;
    case Protocol::Command::CMD_JOIN_ROOM:
        return ActionCommand::JOIN_ROOM;
    case Protocol::Command::CMD_LEAVE_ROOM:
        return ActionCommand::LEAVE_ROOM;
    case Protocol::Command::CMD_SEND_MESSAGE:
        return ActionCommand::SEND_MESSAGE;
    case Protocol::Command::CMD_LOAD_MESSAGE:
        return ActionCommand::REQUEST_DB_LOAD_MESSAGE;
    }
}