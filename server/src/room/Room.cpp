#include "Room.h"
#include <algorithm>

Room::Room(uint32_t room_id, int max_sessions) : room_id_(room_id), max_sessions_(max_sessions), current_sessions_(0) {}
Room::~Room() {}

bool Room::join_session(std::shared_ptr<Session> session) {
    // 정원 체크
    if(current_sessions_ >= max_sessions_)
        return false;

    sessions_.emplace(session->get_fd(), session);
    current_sessions_++;

    return true;
}

bool Room::exit_session(int fd) {
    auto itr = sessions_.find(fd);
    if(itr == sessions_.end())
        return  false;
    
    sessions_.erase(fd);
    current_sessions_--;

    return true;
}

bool Room::find_session(int fd) {
    auto itr = sessions_.find(fd);

    if(itr != sessions_.end())
        return true;
    else
        return false;
}