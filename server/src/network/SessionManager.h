#pragma once

#include <unordered_map>
#include <memory>

#include "Session.h"
#include "Socket.h"

// Server class에서 Session 소유 및 관리 클래스
class SessionManager {
private:
    std::unordered_map<int, std::shared_ptr<Session>> sessions_;

public:
    SessionManager();
    ~SessionManager();

    void add_session(Socket&& sock);
    void remove_session(int fd);
    std::shared_ptr<Session> get_session(int fd);
};