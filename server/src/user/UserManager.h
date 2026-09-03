#pragma once

#include "User.h"
#include "Type.h"

#include <memory>
#include <unordered_map>
#include <mutex>

class UserManager {
private:
    std::unordered_map<UserId, std::shared_ptr<User>> users_;
    std::unordered_map<int, UserId> fd_to_user_id_;
    std::mutex mutex_;

public:
    UserManager();
    ~UserManager();

    void add_user(User&& user);
    void remove_user(UserId user_id);
    std::shared_ptr<User> get_user(UserId user_id);
    std::shared_ptr<User> get_user(int fd);
    UserId get_user_id(int fd);
};