#include "Socket.h"

Socket::Socket(int fd): fd_(fd) {
    if (fd_ < 0) throw std::runtime_error("socket failed");
}
Socket::~Socket() { if (fd_ >= 0) ::close(fd_); }

int Socket::get() const { return fd_; }
int Socket::release() { int f = fd_; fd_ = -1; return f; } // 소유권 이전


ssize_t Socket::recv(char* buf, size_t len){
    return ::recv(fd_, buf, len, 0);
}

ssize_t Socket::send(const char* buf, size_t len){
    return ::send(fd_, buf, len, 0);
}

// 이동 허용
Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1; // 원본 소유권 해제
    // send_buf_.swap(other.send_buf_);
} 
Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if(fd_ >= 0) ::close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}