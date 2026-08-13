#pragma once

#include "IServerService.h"

#include "Epoll.h"
#include "SessionManager.h"
#include "RoomManager.h"
#include "MainEvent.h"

#include <unordered_map>
#include <vector>
#include <mutex>

/*
중앙 클래스 (Main Thread)
새로운 접속 및 패킷 수신
*/
class Server : public IServerService{
private:
    int port_;
    Socket listener_;
    Epoll epoll_;
    int eventfd_;
    std::vector<epoll_event> epoll_events_;
    std::queue<MainEvent> main_events_;
    SessionManager session_manager_;
    UserManager user_manager_;
    RoomManager room_manager_;
    u_long total_users_;
    std::mutex main_event_queue_mutex_;

public:
    Server(int port = 8080, int max_events = 1024);  // 서버 초기화
    ~Server();

    void run(); // 서버 실행

private:
    Socket create_listner(int port);        // 리스너 소켓 생성

    void accept_client();                   // 새 클라이언트 연결 처리
    void handle_request(int fd);            // 요청받은 Request 처리
    void handle_events();
    void handle_epollout(int fd);

    void close_session(int fd);        // 클라이언트 연결 끊기
    void enable_epollout(int fd);           // epoll out 활성화
    void disable_epollout(int fd);          // epoll out 비활성화

public:
    void request_enable_epollout(int fd) override;
    void request_disable_epollout(int fd) override;
    void request_close_session(int fd) override;
};