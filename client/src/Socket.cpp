#include "Socket.h"

#include <cerrno>

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
    return ::send(fd_, buf, len, MSG_NOSIGNAL);
}

void Socket::pack(const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    send_buf_.insert(send_buf_.end(), ptr, ptr + len);
}

bool Socket::flush() {
    if (send_buf_.empty())
        return true;

    bool ok = send_all(send_buf_.data(), send_buf_.size());
    send_buf_.clear();
    return ok;
}

bool Socket::send_all(const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t bytes = ::send(fd_, ptr, remaining, MSG_NOSIGNAL);
        if (bytes > 0) {
            ptr += bytes;
            remaining -= bytes;
        }
        else if (bytes < 0 && errno == EINTR)
            continue;                   // 시그널로 중단된 것은 재시도
        else
            return false;               // 실제 오류 또는 연결 종료
    }
    return true;
}

bool Socket::recv_all(void* buf, size_t len){
    char* ptr = static_cast<char*>(buf);
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t bytes = ::recv(fd_, ptr, remaining, 0);
        if (bytes > 0) {
            ptr += bytes;
            remaining -= bytes;
        }
        else if (bytes == 0)
            return false;               // 서버가 연결을 닫음
        else if (errno == EINTR)
            continue;
        else
            return false;               // 실제 오류
    }
    return true;
}

void Socket::buf_clear() { send_buf_.clear(); }

// 이동 허용
Socket::Socket(Socket&& other) noexcept : fd_(other.fd_), send_buf_(std::move(other.send_buf_)) {
    other.fd_ = -1; // 원본 소유권 해제
}
Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if(fd_ >= 0) ::close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
        send_buf_ = std::move(other.send_buf_);
    }
    return *this;
}
