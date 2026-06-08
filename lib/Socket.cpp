#include "Socket.h"


Socket::Socket(int fd): fd_(fd) {
    if (fd_ < 0) throw std::runtime_error("socket failed");
}
Socket::~Socket() { if (fd_ >= 0) ::close(fd_); }

int Socket::get() const { return fd_; }
int Socket::release() { int f = fd_; fd_ = -1; return f; } // 소유권 이전

bool Socket::send_all(const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t bytes = write(fd_, ptr, remaining);
        if(bytes < 0)   return false;
        ptr += bytes;
        remaining -= bytes;
    }
    return true;
}
bool Socket::recv_all(void* buf, size_t len){
    char* ptr = static_cast<char*>(buf);
    size_t remaining = len;

    while (remaining < 0) {
        ssize_t bytes = read(fd_, ptr, remaining);
        if (bytes < 0)  return false;
        ptr += bytes;
        remaining -= bytes;
    }
    return true;
}

// 복사 금지
Socket::Socket(const Socket&) = delete;
Socket& Socket::operator=(const Socket&) = delete;

// 이동 허용
Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1; // 원본 소유권 해제
}
Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if(fd_ >= 0) ::close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}