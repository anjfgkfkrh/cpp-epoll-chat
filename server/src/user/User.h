#pragma once

#include <string>
#include <atomic>

// 유저 정보를 관리하는 데이터 컨테이너 클래스
class User {
private:
    int fd_;
    uint16_t user_id_;
    std::string user_name_;
    std::atomic<uint16_t> current_room_;
    std::atomic<bool> disconnecting;
public:
    User(int fd, uint16_t user_id, std::string user_name);
    ~User();

    void set_name(std::string user_name);
    const std::string& get_name();

    int get_fd();
    uint16_t get_id();

    inline void set_current_room(uint16_t room_id) { current_room_.store(room_id); }
    inline uint16_t get_current_room() { return current_room_.load(); }
    inline void mark_diconnecting() { disconnecting.store(true); }
    inline bool is_diconnecting() { return disconnecting.load(); }

    // 복사 금지
    User(const User&) = delete;
    User& operator=(const User&) = delete;

    // 이동 허용
    User(User&& other) noexcept;
    User& operator=(User&& other) noexcept;
};