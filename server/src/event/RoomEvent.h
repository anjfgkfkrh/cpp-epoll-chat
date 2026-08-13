#pragma once

#include "Protocol.h"
#include "Result.h"
#include "EventState.h"
#include <memory>
#include <array>

enum class ActionCommand : uint8_t {
    CREATE_ROOM,
    JOIN_ROOM,
    LEAVE_ROOM,
    SEND_MESSAGE,
    
    REQUEST_JOIN_ROOM,
    REQUEST_LEAVE_ROOM,

    SEND_RESPONSE,
    FLUSH,  // epoll out 상황에서의 flush 요청

    DISCONNECT_SESSION,
};

struct RoomAction {
    ActionCommand command;
    uint32_t target_room_id;
};

struct RoomEvent {
    std::array<RoomAction, 10> actions;     // 처리 해야할 행동 리스트
    uint8_t current_action = 0;             // 현재 action
    uint8_t action_len = 0;                 // action 갯수
    uint32_t target_room_id = 0;            // 이벤트 요청 목표 Room ID

    Protocol::Command initial_command;      // 사용자가 보낸 최초 명령

    int fd = -1;                                // disconnect_session을 위한 변수
    std::shared_ptr<Session> session = nullptr; // Session
    std::vector<std::byte> data = {};           // body data

    Result result_code = Result::None;          // 처리 결과 코드

    bool add_action(ActionCommand cmd, uint32_t room_id=0) {
        if(action_len >= actions.size())
            return false;

        actions[action_len++] = {cmd, room_id};

        return true;
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
    }
}