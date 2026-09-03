#include "User.h"

User::User(int fd, UserId user_id, std::string user_name) : fd_(fd), user_id_(user_id), user_name_(std::move(user_name)), disconnecting(false), current_room_(1) { }
User::~User() { }

void User::set_name(std::string user_name) { user_name_ = std::move(user_name); }

int User::get_fd() { return fd_; }

UserId User::get_id() { return user_id_; }

const std::string& User::get_name() { return user_name_; }

User::User(User&& other) noexcept : fd_(other.fd_), user_id_(other.user_id_), user_name_(std::move(other.user_name_)), current_room_(other.current_room_.load()), disconnecting(other.disconnecting.load()) {
    other.fd_ = -1;
}

User& User::operator=(User&& other) noexcept {
    if (this != &other) {
        fd_ = other.fd_;
        other.fd_ = -1;
        user_id_ = other.user_id_;
        user_name_ = std::move(other.user_name_);
        current_room_.store(other.current_room_.load());
        disconnecting.store(other.disconnecting.load());
    }
    return *this;
}