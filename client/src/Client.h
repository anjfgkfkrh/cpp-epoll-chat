#pragma once

#include "Socket.h"
#include "Epoll.h"
#include "Protocol.h"
#include "MessageCodec.h"
#include "Type.h"

#include <string>
#include <optional>
#include <unordered_map>
#include <functional>
#include <sstream>

#define MAX_EVENTS 128

// 로비는 서버가 접속 시 자동으로 넣어주는 방이다.
// 이 번호로 직접 create/join 을 요청하면 서버의 방 이동 플랜이
// "로비 입장 -> 로비 퇴장"이 되어 유저가 어느 방에도 속하지 않는 상태가 된다.
constexpr RoomId LOBBY_ROOM_ID = 1;

enum class ClientState {
    LOBBY,      // 방 생성/입장
    IN_ROOM,    // 채팅, 방 나가기
};

class Client {
private:
    // 서버는 요청 하나당 응답 하나를 보내며, 클라이언트는 그 응답이 올 때까지
    // 다음 요청을 보내지 않는다. JOIN/LEAVE 응답에는 room_id 가 실려오지 않으므로
    // 어떤 방을 대상으로 한 요청이었는지 여기에 기억해 둔다.
    // (CREATE 는 예외로, 서버가 발급한 room_id 가 응답 body 에 실려온다)
    struct Pending {
        Protocol::Command command;
        RoomId room_id;
    };

    std::optional<Socket> sock_;        // 서버 연결 소켓
    Epoll epoll_;                       // epoll
    std::vector<epoll_event> events_;   // 대기중인 epoll 이벤트들
    std::unordered_map<std::string, std::function<void(std::istringstream&)>> commands_lobby_;  // 로비 명령어
    std::unordered_map<std::string, std::function<void(std::istringstream&)>> commands_room_;   // 방 명령어
    ClientState state_;                 // 현재 위치 (로비, 방)
    RoomId room_id_;                  // 현재 접속 중인 room_id
    int64_t oldest_message_id_;         // 지금까지 받은 가장 오래된 메시지 id (0 = 아직 없음)
                                        // 다음 /load 의 커서로 쓴다
    bool running_;                      // 작동 플래그
    std::optional<Pending> pending_;    // 응답 대기중인 요청

public:
    explicit Client(uint16_t port = 8080);   // 서버 연결, epoll 등록, 명령어 등록
    ~Client();
    void run();                         // 작동 시작
    bool is_connected() const;          // 접속 성공 여부

private:
    std::string read_line();            // 키보드 문자열 입력 수신
    bool connect_server(uint16_t port); // 서버 연결
    bool setup_command();               // 명령어 초기화 및 등록
    bool validate_room_id(RoomId room_id) const;  // 요청 전 방 번호 검사 (로비 번호 차단)
    bool send_request(Protocol::Command cmd, RoomId room_id, const std::string& body = "");

    bool handle_input();                // 사용자 입력 처리
    bool handle_packet();               // Packet 처리
    bool handle_response();             // Response 처리
    bool handle_broadcast();            // Broadcast 처리

    // 과거 메시지 body 를 해석해 출력하고 다음 /load 커서를 갱신한다.
    // 반환: 출력한 메시지 수, -1 이면 파싱 실패
    int  print_history(const std::string& body);

    void print_prompt() const;          // 현재 상태에 맞는 안내 출력
};
