#include "UserManager.h"

UserManager::UserManager() { }
UserManager::~UserManager() { }

void UserManager::add_user(User&& user) {
    std::lock_guard lock(mutex_);

    auto user_ = std::make_shared<User>(std::move(user));

    {
        users_.emplace(user_->get_id(), user_);

        fd_to_user_id_.emplace(user_->get_fd(), user_->get_id());
    }
}

void UserManager::remove_user(uint16_t user_id) {
    {
        std::lock_guard lock(mutex_);

        if(users_.find(user_id) == users_.end())
            return;
        
        std::shared_ptr<User> user =  users_.at(user_id);

        fd_to_user_id_.erase(user->get_fd());

        users_.erase(user_id);
    }
}

std::shared_ptr<User> UserManager::get_user(uint16_t user_id) {
    {
        std::lock_guard lock(mutex_);

        if(users_.find(user_id) == users_.end())
            return nullptr;
        
        return users_.at(user_id);
    }
}

std::shared_ptr<User> UserManager::get_user(int fd) {
    {
        std::lock_guard lock(mutex_);
        
        if(fd_to_user_id_.find(fd) == fd_to_user_id_.end())
            return 0;

        uint16_t user_id = fd_to_user_id_.at(fd);

        if(users_.find(user_id) == users_.end())
            return nullptr;

        return users_.at(user_id);
    }
}

uint16_t UserManager::get_user_id(int fd) {
    {
        std::lock_guard lock(mutex_);
        
        if(fd_to_user_id_.find(fd) == fd_to_user_id_.end())
            return 0;

        return fd_to_user_id_.at(fd);
    }
}