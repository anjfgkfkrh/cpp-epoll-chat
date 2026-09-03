#pragma once

#include "Type.h"

#include <string>
#include <atomic>

// 유저 정보를 관리하는 데이터 컨테이너 클래스
class User {
private:
    int fd_;
    UserId user_id_;
    std::string user_name_;
    std::atomic<RoomId> current_room_;
    std::atomic<bool> disconnecting;
public:
    User(int fd, UserId user_id, std::string user_name);
    ~User();

    void set_name(std::string user_name);
    const std::string& get_name();

    int get_fd();
    UserId get_id();

    inline void set_current_room(RoomId room_id) { current_room_.store(room_id); }
    inline RoomId get_current_room() { return current_room_.load(); }
    inline void mark_diconnecting() { disconnecting.store(true); }
    inline bool is_diconnecting() { return disconnecting.load(); }

    // 복사 금지
    User(const User&) = delete;
    User& operator=(const User&) = delete;

    // 이동 허용
    User(User&& other) noexcept;
    User& operator=(User&& other) noexcept;
};