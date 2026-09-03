#include "Server.h"

#include "Protocol.h"
#include "RoomEvent.h"

#include <iostream>
#include <stdexcept>
#include <sys/eventfd.h>

Server::Server(int port, int max_event): port_(port), listener_(create_listner(port)), db_(DBWorker()), room_manager_(RoomManager(user_manager_, db_, *this)), total_users_(0) {
    epoll_.add(listener_.get(), EPOLLIN);
    epoll_events_.resize(max_event);

    eventfd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (eventfd_ == -1)
    {
        perror("eventfd");
        exit(EXIT_FAILURE);
    }

    epoll_.add(eventfd_, EPOLLIN);

    std::cout << "[info] Listening on port " << port_ << std::endl;
}

Server::~Server() {
    room_manager_.stop();
    db_.stop();
    ::close(eventfd_);
}

void Server::run() {
    std::cout << "[info] running Server" << std::endl;

    // worker thread 시작
    db_.start();
    room_manager_.start();

    while(true) {
        int n = epoll_.wait(epoll_events_);
        std::cout << "[info] new epoll in: " << n << std::endl;

        for(int i=0; i<n; i++){
            int fd = epoll_events_[i].data.fd;
            uint32_t events = epoll_events_[i].events;

            if(fd == listener_.get()){
                accept_client();
                continue;
            }

            if(fd == eventfd_){
                handle_events();
                continue;
            }

            if(events & EPOLLIN)
                handle_request(fd);

            if(events & EPOLLOUT)
                handle_epollout(fd);

            if(events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
                close_session(fd);
        }
    }
}

Socket Server::create_listner(int port) {
    Socket sock(socket(AF_INET, SOCK_STREAM, 0));

    int opt = 1;
    setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);


    if (bind(sock.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("[error] bind failed");
    if(listen(sock.get(), 128) < 0)
        throw std::runtime_error("[error] listen failed");

    return sock;
}

void Server::accept_client() {
    int fd = accept4(listener_.get(), nullptr, nullptr, SOCK_NONBLOCK);
    if(fd < 0)
        return;

    epoll_.add(fd, EPOLLIN | EPOLLET | EPOLLRDHUP);
    
    // Socket, Session 생성
    Socket sock(fd);
    session_manager_.add_session(std::move(sock));

    // User 생성
    User user(fd, ++total_users_, "temp");   // 임시 user_id, user_name
    user_manager_.add_user(std::move(user));

    // lobby 입장
    RoomEvent event;
    event.target_room_id = LOBBY_ROOM_ID;
    event.session = session_manager_.get_session(fd);
    event.add_action(ActionCommand::JOIN_ROOM, LOBBY_ROOM_ID);
    room_manager_.route_event(std::move(event), LOBBY_WORKER_ID);

    std::cout << "[info] New User: " << total_users_-1 << " fd: " << fd << std::endl;
}


void Server::handle_events() {
    eventfd_t value;
    eventfd_read(eventfd_, &value);

    MainEvent event;
    do{
        {   
            std::lock_guard lock(main_event_queue_mutex_);
            if(main_events_.empty())
                return;
            event = std::move(main_events_.front());
            main_events_.pop();
        }

        switch(event.type) {
        case MainEventType::Disconnect:
            close_session(event.fd);
            break;
        case MainEventType::EnableEpollOut:
            enable_epollout(event.fd);
            break;
        case MainEventType::DisalbeEpollOut:
            disable_epollout(event.fd);
            break;
        }
    } while(true);
}


void Server::handle_request(int fd) {
    // 해당 세션의 on_readable() 호출
    // 모두 수신시 extrate_packet() 호출
    // RoomManager에게 dispatch_packet() 호출하여 Packet, Session 넘김

    std::shared_ptr<Session> session = session_manager_.get_session(fd);
    if(session == nullptr || !session->on_readable()){
        close_session(fd);  // 수신 오류로 인한 Session 해제
        return;
    }

    Packet packet;
    while(session->extract_packet(packet)) {    // 한번에 2개 이상의 packet이 수신될 경우를 위한 반복문
        room_manager_.dispatch_packet(std::move(packet), session);  // 모든 데이터 수신 완료 후 RoomManager에게 넘김
        packet = {};    // 다시 쓰기위해 초기화
    }
}


void Server::handle_epollout(int fd) {
    std::shared_ptr<Session> session = session_manager_.get_session(fd);
    if(session == nullptr) {
        close_session(fd);  // 수신 오류로 인한 Session 해제
        return;
    }

    room_manager_.request_flush(session);
}

void Server::close_session(int fd) {
    // 1. epoll 등록 해제
    // 2. 소속된 Room에서 제외
    // 3. Session 삭제
    // 4. User 삭제

    // 이미 삭제되었으면 조기 종료
    if(session_manager_.get_session(fd) == nullptr)
        return;

    // epoll 등록 해제
    epoll_.remove(fd);
    
    std::shared_ptr<User> user = user_manager_.get_user(fd);
    std::shared_ptr<Session> session = session_manager_.get_session(fd);
    user->mark_diconnecting();
    session->mark_disconnecting();

    // 소속된 Room에서 제거, 안전하게 로비도 확인
    RoomId room_id = user->get_current_room();
    room_manager_.disconnect_session(fd, room_id);
    if(room_id != LOBBY_ROOM_ID)
        room_manager_.disconnect_session(fd, LOBBY_ROOM_ID);
    
    // Session 삭제
    session_manager_.remove_session(fd);

    // User 삭제
    user_manager_.remove_user(user->get_id());
} 


void Server::enable_epollout(int fd) {
    epoll_.enable_epoll_out(fd);
}

void Server::disable_epollout(int fd) {
    epoll_.disable_epoll_out(fd);
}


void Server::request_enable_epollout(int fd) {
    MainEvent event;
    event.type = MainEventType::EnableEpollOut;
    event.fd = fd;

    {
        std::lock_guard lock(main_event_queue_mutex_);

        main_events_.push(std::move(event));
    }

    eventfd_write(eventfd_, 1);
}

void Server::request_disable_epollout(int fd) {
    MainEvent event;
    event.type = MainEventType::DisalbeEpollOut;
    event.fd = fd;

    {
        std::lock_guard lock(main_event_queue_mutex_);

        main_events_.push(std::move(event));
    }

    eventfd_write(eventfd_, 1);
}

void Server::request_close_session(int fd) {
    MainEvent event;
    event.type = MainEventType::Disconnect;
    event.fd = fd;

    {
        std::lock_guard lock(main_event_queue_mutex_);

        main_events_.push(std::move(event));
    }

    eventfd_write(eventfd_, 1);
}