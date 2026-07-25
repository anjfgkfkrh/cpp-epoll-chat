#include <vector>

#include "Socket.h"
#include "Protocol.h"

// 클라이언트와 서버의 1대1 연결 담당
class Session {
private:
    std::vector<char> recv_buffer_;
    std::vector<char> send_buffer_;
    Socket sock_;

public:
    Session(Socket&& sock);
    ~Session();

    inline int get_fd() { return sock_.get(); };

    bool on_readable();
    bool extract_packet(Packet& packet);

    void disconnect();
};