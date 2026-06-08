#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

class Socket {
private:
    int fd_;
public:
    explicit Socket(int fd);
    ~Socket();

    int get() const;
    int release();
    bool send_all(const void* data, size_t len);    // 데이터 송신
    bool recv_all(void* buf, size_t len);   // 데이터 수신

    // 복사 금지
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // 이동 허용
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
};