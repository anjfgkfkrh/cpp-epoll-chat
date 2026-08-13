#pragma once

#include <iostream>
#include <vector>
#include <sys/epoll.h>
#include <unistd.h>

// Epoll을 관리하는 클래스
// RAII epoll
class Epoll {
private:
    int fd_;
public:
    Epoll();
    ~Epoll();

    void add(int target_fd, uint32_t events);
    void remove(int target_fd);
    int wait(std::vector<epoll_event>& events, int timeout = -1);

    void enable_epoll_out(int fd);
    void disable_epoll_out(int fd);

    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;
};