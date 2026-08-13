#include "Epoll.h"


Epoll::Epoll() : fd_(epoll_create1(0)) {
    if (fd_ < 0) throw std::runtime_error("epoll_create1 failed");
}
Epoll::~Epoll() { if (fd_ >= 0) ::close(fd_); }

void Epoll::add(int target_fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = target_fd;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, target_fd, &ev) < 0)
        throw std::runtime_error("epoll_ctl ADD failed");
}

void Epoll::remove(int target_fd){
    epoll_ctl(fd_, EPOLL_CTL_DEL, target_fd, nullptr);
}

int Epoll::wait(std::vector<epoll_event>& events, int timeout) {
    int n = epoll_wait(fd_, events.data(), events.size(), timeout);
    if (n < 0) throw std::runtime_error("epoll_wait failed");
    return n;
}
