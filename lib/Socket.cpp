#include "Socket.h"


Socket::Socket(int fd): fd_(fd) {
    if (fd_ < 0) throw std::runtime_error("socket failed");
}
Socket::~Socket() { if (fd_ >= 0) ::close(fd_); }

int Socket::get() const { return fd_; }
int Socket::release() { int f = fd_; fd_ = -1; return f; } // 소유권 이전

void Socket::pack(const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    send_buf_.insert(send_buf_.end(), ptr, ptr + len);
}

bool Socket::flush() {
    if(!send_all(send_buf_.data(), send_buf_.size()))
        return false;
    send_buf_.clear();
    return true;
}

bool Socket::send_all(const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t bytes = write(fd_, ptr, remaining);
        std::cerr << "[debug] write returned: " << bytes << ", errno: " << errno << std::endl;
        if(bytes < 0)   return false;
        ptr += bytes;
        remaining -= bytes;
    }
    return true;
}

bool Socket::recv_all(void* buf, size_t len){
    std::cout << "[debug][recv] ready to recv len: " << len << std::endl;
    char* ptr = static_cast<char*>(buf);
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t bytes = read(fd_, ptr, remaining);
        if (bytes <= 0)  return false;
        ptr += bytes;
        remaining -= bytes;
    }
    std::cout << "[debug][recv] recv all len: " << len << std::endl;
    return true;
}

void Socket::buf_clear() { send_buf_.clear(); }

// 이동 허용
Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1; // 원본 소유권 해제
    send_buf_.swap(other.send_buf_);
} 
Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if(fd_ >= 0) ::close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}