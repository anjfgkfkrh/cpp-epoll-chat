#pragma once

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <vector>

// 실제 데이터 전송 및 Socket 관리 클래스
class Socket {
private:
    int fd_;
    std::vector<char> send_buf_;
public:
    explicit Socket(int fd);
    ~Socket();

    int get() const;
    int release();
    void pack(const void* data, size_t len);    // 송신할 데이터 버퍼에 저장
    bool flush();                               // 버퍼에 있는 모든 데이터 송신
    bool send_all(const void* data, size_t len);    // 데이터 송신
    bool recv_all(void* buf, size_t len);           // 데이터 수신
    void buf_clear();

    // 복사 금지
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // 이동 허용
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
};