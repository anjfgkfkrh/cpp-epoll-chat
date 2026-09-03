#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include "Protocol.h"
#include "Session.h"
#include "Type.h"

// 룸 정보를 관리하는 데이터 컨테이너 클래스
class Room {
private:
    RoomId room_id_;
    std::unordered_map<int, std::shared_ptr<Session>> sessions_;
    int max_sessions_;
    int current_sessions_ = 0;
public:
    explicit Room(RoomId room_id, int max_sessions=5);
    ~Room();

    // 복사 방지
    Room(Room&&) = default;
    Room& operator=(Room&&) = default;

    bool join_session(std::shared_ptr<Session> session); // 클라이언트 방 입장
    bool exit_session(int fd);                           // 클라이언트 방 퇴장
    bool find_session(int fd);                           // 클라이언트 찾기
    inline RoomId get_room_id() { return room_id_; }
    inline std::unordered_map<int, std::shared_ptr<Session>>& get_sessions() { return sessions_; }  // 클라이언트들 반환
    inline int get_sessions_num() { return current_sessions_; }
};