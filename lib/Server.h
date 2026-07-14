#include "Socket.h"
#include "Epoll.h"
#include "Room.h"
#include "User.h"

#include <unordered_map>
#include <vector>

// 실제 서버 로직을 담당하는 중앙 클래스
class Server {
private:
    int port_;
    Socket listener_;
    Epoll epoll_;
    std::vector<epoll_event> events_;
    std::unordered_map<uint16_t, Room> rooms_;
    std::unordered_map<int, Socket> sockets_;
    std::unordered_map<int, User> users_;
    std::unordered_map<int, uint16_t> fd_to_userid_;
    Room lobby_;
    u_long total_users_;

public:
    Server(int port = 8080, int max_events = 64);  // 서버 초기화
    ~Server();

    void run(); // 서버 실행

private:
    Socket create_listner(int port);        // 리스너 소켓 생성
    void accept_client();                   // 새 클라이언트 연결 처리

    void disconnect_client(int fd);         // 클라이언트 연결 끊기
    //void disconnect_client(uint16_t user_id);

    void handle_request(int fd);            // 요청받은 Request 처리

    // Request 함수들
    bool create_room(int fd, uint16_t room_id);           // Room 생성
    bool join_room(int fd, uint16_t room_id);             // Room에 client 삽입
    bool leave_room(int fd, uint16_t room_id);            // Room에 client 제거
    bool send_message(int sender_fd, uint16_t room_id, const std::string& message);   // Room에 message broadcast

    void send_Response(int fd, uint16_t cmd, uint16_t status, uint16_t body_len = 0, const void* body = NULL);

    // bool send_packet(int user_id, void* data, size_t len); // Socket:pack()을 사용함으로 더이상 사용 안함
};