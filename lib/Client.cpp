#include "Client.h"

#include <iostream>
#include <sstream>
#include "Protocol.h"

#define BUF_SIZE 1024

Client::Client() {

    // 명령어 설정
    setup_command();

    // 서버 접속
    int try_num = 0;
    do
    {
        std::cout << "[info] Try connect to server: " << try_num << std::endl;
        try_num++;
        if(connect_server())
            break;
        std::cout << "[error] Fail to connect to server" << std::endl; 
    } while (try_num <= 3);
    if(try_num > 3) return;
    
    std::cout << "[info] Success to connect Server!";

    // 상태 설정
    state_ = ClientState::LOBBY;

    // Epoll 등록
    epoll_.add(sock_->get(), EPOLLIN);
    epoll_.add(STDIN_FILENO, EPOLLIN);

    // running flag 초기화
    running_ = false;
}


bool Client::connect_server() {
    sock_ = Socket(socket(AF_INET, SOCK_STREAM, 0));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 내부 테스트용
    addr.sin_port = htonl(8080);

    if (connect(sock_->get(), (struct sockaddr *)&addr, sizeof(addr)) < 0){
        perror("connect");
        return false;
    }

    return true;
}

bool Client::setup_command() {
    commands_lobby_["/create"] = [this](std::istringstream& args) {
        uint16_t room_id;
        if (args >> room_id) {
            if(send_request(CMD_CREATE_ROOM, room_id))      std::cout << "[info] Create Room Request sended" << std::endl;
            else    std::cout << "[error] Fail to send Request" << std::endl;
        } else {
            std::cout << "[error] please write '/create [room_id]'" << std::endl;
        }
    };

    commands_lobby_["/join"] = [this](std::istringstream& args) {
        uint16_t room_id;
        if (args >> room_id) {
            if (send_request(CMD_JOIN_ROOM, room_id))       std::cout << "[info] Join the Room Request sended" << std::endl;
            else    std::cout << "[error] Fail to send Request" << std::endl;
        } else {
            std::cout << "[error] please write '/join [room_id]'" << std::endl;
        }
    };

    commands_lobby_["/exit"] = [this](std::istringstream& args) {
        std::cout << "[info] Program exit" << std::endl;
        running_ = false;
    };

    commands_room_["/leave"] = [this](std::istringstream& args) {
        if (send_request(CMD_LEAVE_ROOM, room_id_))     std::cout << "[info] Leave the Room Request sended" << std::endl;
        else    std::cout << "[error] Fail to send Request" << std::endl;
    };
}

void Client::run(){
    running_ = true;

    while(running_) {
        int n = epoll_.wait(events_);
        
        for (int i=0; i<n && running_; i++){
            int event = events_[i].data.fd;

            if (event == STDIN_FILENO)
                handle_input();
            else if (event == sock_->get())
                handle_packet();
        }
    }
}


std::string Client::read_line() {
    char buf[1024];
    int bytes = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (bytes <= 0) return "";

    // 개행 제거
    if (bytes > 0 && buf[bytes - 1 == '\n'])
        bytes--;

    return std::string(buf, bytes);
}

bool Client::send_request(Command cmd, uint16_t room_id, const std::string& body = "") {
    Request req;
    req.command = cmd;
    req.room_id = room_id;
    req.body_len = body.size();

    if(!sock_->send_all(&req, sizeof(Request)))
        return false;
    if (body.size() > 0 && !sock_->send_all(body.data(), body.size()))
        return false;

    return true;
}

bool Client::handle_input(){
    if(pending_command_){
        std::cout << "[info] Processing request..." << std::endl;
        return false;
    }

    std::string input = read_line();
    std::istringstream stream(input);
    std::string cmd;
    stream >> cmd;

    if (state_ == ClientState::LOBBY){
        auto itr = commands_lobby_.find(cmd);
        if (itr != commands_lobby_.end())   itr->second(stream); // 로비 명령어 실행
        else    std::cout << "[error] This command does not exist." << std::endl;
    } else if (state_ == ClientState::IN_ROOM){
        auto itr = commands_room_.find(cmd);
        if (itr != commands_room_.end())    itr->second(stream); // 룸 명령어 실행
        else {
            if(!send_request(CMD_SEND_MESSAGE, room_id_, input))
                std::cout << "[error] Fail to send message" << std::endl;
        }
    }
}

bool Client::handle_packet(){
    Header header;
    if (!sock_->recv_all(&header, sizeof(Header)))
        return false;

    switch(header.type)
    {
    case PKT_RESPONSE:  handle_response();  break;
    case PKT_BROADCAST: handle_broadcast(); break;
    default:
        break;
    }
}

bool Client::handle_response() {    
    // TODO: 실패 코드 반환 -> 코드에 따른 처리 or 내부적으로 처리
    // 함수 종료시 자동으로 pending_command_ 리셋
    auto guard = [this]() { pending_command_.reset(); };
    struct ScopeGuard {
        std::function<void()> fn;
        ~ScopeGuard() { fn(); }
    } scope_guard{guard};

    Response res;

    if (!sock_->recv_all(&res, sizeof(Response)))    // 패킷 무결성 검사
        return false;
    if (pending_command_ != res.command)     // 대기중인 요청 비교
        return false;
    if (res.status != 0){        // 요청 실패
        std::string body(res.body_len, '\0');
        if (!sock_->recv_all(body.data(), res.body_len))
            return false;
        std::cout << "[error] Request Fail: " << body << std::endl;
        return false;
    }

    switch(res.command)
    {
    case CMD_CREATE_ROOM:{
        if (res.body_len == sizeof(uint16_t))
            return false;
        if (!sock_->recv_all(&room_id_, sizeof(res.body_len)))
            return false;
        if (state_ != ClientState::LOBBY)
            return false;

        state_ = ClientState::IN_ROOM;

        std::cout << "[info] Create Room " << room_id_ << std::endl;
        std::cout << "[info] Join Room " << room_id_ << std::endl;
        break;
    }
    case CMD_JOIN_ROOM: {
        if (res.body_len == sizeof(uint16_t))
            return false;
        if (!sock_->recv_all(&room_id_, sizeof(res.body_len)))
            return false;
        if (state_ != ClientState::LOBBY)
            return false;

        state_ = ClientState::IN_ROOM;

        std::cout << "[info] Join Room " << room_id_ << std::endl; 
        break;
    }
    case CMD_LEAVE_ROOM: {
        if (state_ != ClientState::IN_ROOM)
            return false;

        state_ = ClientState::LOBBY;

        std::cout << "[info] Leave Room " << room_id_ << std::endl;
        room_id_ = NULL;
        break;
    }
    }
    return true;
}

bool Client::handle_broadcast() {   
    // TODO: 실패 코드 반환 -> 코드에 따른 처리 or 내부적으로 처리
    if(state_ == ClientState::LOBBY)    // 임시 조치 -> 현재 로비에서 받을 braodcast 없음
        return false;
    
    Broadcast broad;

    if (!sock_->recv_all(&broad, sizeof(Broadcast)))
        return false;
    if (broad.room_id != room_id_)
        return false;
            
    std::string body(broad.body_len, '\0');
    if (!sock_->recv_all(body.data(), broad.body_len))
        return false;
            
    std::cout << broad.sender_id << ": " << body << std::endl; // 임시 조치 -> 이벤트에 따른 분기 처리
    return true;
}