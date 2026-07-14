#include "Socket.h"
#include "Epoll.h"
#include "Protocol.h"

#include <string>
#include <optional>
#include <unordered_map>
#include <functional>

#define MAX_EVENTS 128

enum class ClientState {
    LOBBY,      // 방 생성/입장
    IN_ROOM,    // 채팅, 방 나가기
};

class Client {
private:
    std::optional<Socket> sock_;        // 서버 연결 소켓
    Epoll epoll_;                       // epoll
    std::vector<epoll_event> events_;   // 대기중인 epoll 이벤트들
    std::unordered_map<std::string, std::function<void(std::istringstream&)>> commands_lobby_;  // 로비 명령어
    std::unordered_map<std::string, std::function<void(std::istringstream&)>> commands_room_;   // 방 명령어
    ClientState state_;         // 현재 위치 (로비, 방)
    uint16_t room_id_;          // 현재 접속 중인 room_id
    bool running_;              // 작동 플래그
    std::optional<uint16_t> pending_command_;   // 응답 대기중인 요청


public:
    Client();   // 서버 연결, epoll 등록, 명령어 등록, 작동 플래그 초기화
    ~Client();  // 소멸자
    void run(); // 작동 시작

private:
    std::string read_line();    // 키보드 문자열 입력 수신
    bool connect_server();      // 서버 연결
    bool setup_command();       // 명령어 초기화 및 등록
    bool send_request(Command cmd, uint16_t room_id, const std::string& msg = ""); // Request 요청 송신

    bool handle_input();        // 사용자 입력 처리
    bool handle_packet();       // Packet 처리
    bool handle_response();     // Response 처리
    bool handle_broadcast();    // Broadcast 처리
};