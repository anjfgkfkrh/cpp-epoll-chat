#pragma once

#include <vector>
#include <queue>
#include <atomic>

#include "Socket.h"
#include "Protocol.h"

using Protocol::Packet;

// 클라이언트와 서버의 1대1 연결 담당
class Session {
private:
    struct SendBuffer {
        std::vector<std::byte> buffer;
        size_t offset = 0;
    };

public:
    enum class FlushResult {
        Complete,   // 전송 완료
        Pending,    // 일부만 전송
        Error       // 에러
    };

private:
    std::vector<char> recv_buffer_;
    std::queue<SendBuffer> send_queue_;
    Socket sock_;
    std::atomic<bool> disconnecting_;

public:
    Session(Socket&& sock);
    ~Session();

    inline int get_fd() { return sock_.get(); };

    bool on_readable();
    bool extract_packet(Packet& packet);

    void pack(std::vector<std::byte> data);
    FlushResult flush();

    inline void mark_disconnecting() { disconnecting_.store(true); }
    inline bool is_disconnecting() { return disconnecting_.load(); }
};
