#include "SessionManager.h"

SessionManager::SessionManager() {}
SessionManager::~SessionManager() {
    sessions_.clear();
}

void SessionManager::add_session(Socket&& sock) {
    auto session = std::make_shared<Session>(std::move(sock));

    sessions_.emplace(session->get_fd(), session);
}

void SessionManager::remove_session(int fd) {
    sessions_.erase(fd);
}

std::shared_ptr<Session> SessionManager::get_session(int fd) {
    auto itr = sessions_.find(fd);

    if(itr == sessions_.end())
        return nullptr;
    
    return itr->second;
}