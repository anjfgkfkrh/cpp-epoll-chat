#pragma once

#include "User.h"

#include <memory>
#include <unordered_map>
#include <mutex>

class UserManager {
private:
    std::unordered_map<uint16_t, std::shared_ptr<User>> users_;
    std::unordered_map<int, uint16_t> fd_to_user_id_;
    std::mutex mutex_;

public:
    UserManager();
    ~UserManager();

    void add_user(User&& user);
    void remove_user(uint16_t user_id);
    std::shared_ptr<User> get_user(uint16_t user_id);
    std::shared_ptr<User> get_user(int fd);
    uint16_t get_user_id(int fd);
};