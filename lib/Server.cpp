#include "Server.h"

#include <iostream>
#include <stdexcept>

// TODO: lobby 재설계 필요
Server::Server(int port, int max_event): port_(port), listener_(create_listner(port)), lobby_(Room(0, 20)), total_users_(0) {
    epoll_.add(listener_.get(), EPOLLIN);
    events_.resize(max_event);

    std::cout << "[info] Listening on port " << port_ << std::endl;
}

Server::~Server() {

}

void Server::run() {
    std::cout << "[info] running Server" << std::endl;

    while(true) {
        int n = epoll_.wait(events_);
        std::cout << "[info] new epoll in: " << n << std::endl;

        for(int i=0; i<n; i++){
            int fd = events_[i].data.fd;

            if(fd == listener_.get())
                accept_client();
            else
                handle_request(fd);
        }
    }
}

Socket Server::create_listner(int port) {
    Socket sock(socket(AF_INET, SOCK_STREAM, 0));

    int opt = 1;
    setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
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
    int fd = accept(listener_.get(), nullptr, nullptr);
    if(fd < 0)
        return;
    epoll_.add(fd, EPOLLIN);

    Socket sock(fd);
    sockets_.emplace(fd, std::move(sock));
    User user(fd, ++total_users_, "temp");   // 임시 user_id, user_name
    users_.emplace(user.get_id(), std::move(user));
    fd_to_userid_.emplace(fd, user.get_id());

    lobby_.join_client(user.get_id());

    std::cout << "[info] New User: " << user.get_id() << " fd: " << fd << std::endl;
}

void Server::disconnect_client(int fd) {
    uint16_t id = fd_to_userid_.at(fd);
    // lobby 순회
    if(lobby_.find_client(id)){
        lobby_.exit_client(id);
        std::cout << "[info] disconnect client: " << id << std::endl;
    }
    else{
        // room 순회 // TODO: 모든 방을 순회하는것은 부적절, User 안에 현재 위치하는 room_id를 넣는것이 바람직함
        for(auto& [room_id, room] : rooms_){
            if(room.find_client(id)){
                room.exit_client(id);
                std::cout << "[info] disconnect client: " << id << std::endl;
                break;
            }
        }
    }

    sockets_.erase(fd);
    users_.erase(fd_to_userid_.at(fd));
    fd_to_userid_.erase(fd);
    epoll_.remove(fd);
} 

void Server::handle_request(int fd) {
    auto itr = sockets_.find(fd);   // sockets_에서 fd에 해당하는 Socket을 찾아서 sock에 할당
    if(itr == sockets_.end()) {
        std::cout << "[Error] This is a request from a non-existent client." << std::endl;
        return;
    }
    Socket& sock = itr->second;

    std::cout << "[info] new Request recv" << std::endl;
    
    Header header;
    if(!sock.recv_all(&header, sizeof(Header))){
        disconnect_client(fd);
        return;
    }
    Request request;
    if(!sock.recv_all(&request, sizeof(Request))){
        disconnect_client(fd);
        return;
    }
    std::string body(request.body_len, '\0');
    if(!body.empty())
        if(!sock.recv_all(body.data(), body.size())){
            disconnect_client(fd);
            return;
        }

    std::cout << "[info] request command: " << request.command << std::endl;

    std::string errmsg;
    
    switch (request.command)
    {
    case CMD_CREATE_ROOM:{
        if(create_room(fd, request.room_id))   
            send_Response(fd, CMD_CREATE_ROOM, 0, sizeof(request.room_id), &request.room_id);
        else{
            errmsg = "already exist room id";
            send_Response(fd, CMD_CREATE_ROOM, 1, errmsg.size(), errmsg.data());
        }
        break;
    }
    case CMD_JOIN_ROOM:{
        if(join_room(fd, request.room_id)) 
            send_Response(fd, CMD_JOIN_ROOM, 0, sizeof(request.room_id), &request.room_id);
        else{
            errmsg = "";
            send_Response(fd, CMD_JOIN_ROOM, 1, errmsg.size(), errmsg.data());
        }
        break;
    }
    case CMD_LEAVE_ROOM:
        if(leave_room(fd, request.room_id))    
            send_Response(fd, CMD_LEAVE_ROOM, 0);
        else{
            errmsg = "";
            send_Response(fd, CMD_LEAVE_ROOM, 1, errmsg.size(), errmsg.data());
        }
        break;
    case CMD_SEND_MESSAGE:
        if(send_message(fd, request.room_id, body)) 
            send_Response(fd, CMD_SEND_MESSAGE, 0);
        else{
            errmsg = "";
            send_Response(fd, CMD_SEND_MESSAGE, 1, errmsg.size(), errmsg.data());
        }   
        break;
    }
}

bool Server::create_room(int fd, uint16_t room_id) {
    auto room = rooms_.find(room_id);
    if(room != rooms_.end())
        return false;
    
    Room new_room(room_id);
    lobby_.exit_client(fd_to_userid_.at(fd));
    new_room.join_client(fd_to_userid_.at(fd));

    rooms_.emplace(room_id, std::move(new_room));

    std::cout << "[info] client " << fd_to_userid_.at(fd) << " create room " << room_id << std::endl;
    return true;
}

bool Server::join_room(int fd, uint16_t room_id) {
    auto room = rooms_.find(room_id);
    if(room == rooms_.end())
        return false;

    lobby_.exit_client(fd_to_userid_.at(fd));
    room->second.join_client(fd_to_userid_.at(fd));
    
    std::cout << "[info] client " << fd_to_userid_.at(fd) << " join room " << room_id << std::endl;
    return true;
}

bool Server::leave_room(int fd, uint16_t room_id) {
    auto room = rooms_.find(room_id);
    if(room == rooms_.end())
        return false;
    
    room->second.exit_client(fd_to_userid_.at(fd));
    lobby_.join_client(fd_to_userid_.at(fd));

    
    std::cout << "[info] client " << fd_to_userid_.at(fd) << " leave room " << room_id << std::endl;

    if(room->second.get_clients_num() <= 0) {
        rooms_.erase(room_id);
        std::cout << "[info] erase room " << room_id << std::endl;
        std::cout << "[info] remaining rooms " << rooms_.size() << std::endl;
    }
    return true;
}

bool Server::send_message(int sender_fd, uint16_t room_id, const std::string& message) {
    auto room = rooms_.find(room_id);
    if(room == rooms_.end())
        return false;

    uint16_t sender_id = fd_to_userid_.at(sender_fd);

    // 헤더 생성
    Header header;
    header.type = PKT_BROADCAST;

    // 패킷 조립
    Broadcast packet {
        .event = EVT_MESSAGE,
        .sender_id = sender_id, // 임시(fd)
        .room_id = static_cast<uint16_t>(room_id),
        .body_len = static_cast<uint32_t>(message.size())
    };
    
    // 방에 있는 클라이언트들에게 순차적으로 전송
    for(auto user_id : room->second.clients()){
        if(user_id == sender_id)
            continue;
        Socket& sock = sockets_.at(users_.at(user_id).get_fd());
        sock.pack(&header, sizeof(Header));
        sock.pack(&packet, sizeof(Broadcast));
        sock.pack(message.data(), message.size());
        sock.flush();
    }

    return true;
}

// 여기까지 확인

void Server::send_Response(int fd, uint16_t cmd, uint16_t status, uint16_t body_len, const void* body){
    Socket& sock = sockets_.at(fd);

    Header header;
    header.type = PKT_RESPONSE;
    sock.pack(&header, sizeof(Header));

    Response response;
    response.command = cmd;
    response.status = status;
    response.body_len = body_len;
    sock.pack(&response, sizeof(Response));

    if(body_len > 0)
        sock.pack(body, body_len);

    if(!sock.flush()){
        disconnect_client(fd);
        std::cout << "[error] request failed" << std::endl;
    }

    std::cout << "[info] responsed sended " << cmd << std::endl;
}

// bool Server::send_packet(int user_id, void* data, size_t len) {
//     std::cout << "[debug][send_packet] " << user_id << std::endl;

//     auto user = users_.find(user_id);
//     if(user == users_.end()){
//         std::cout << "[error][send_packet] incorrect user_id " << user_id << std::endl;
//         return false;
//     }

//     auto sock = sockets_.find(user->second.get_fd());
//     if(sock == sockets_.end()){
//         std::cout << "[error][send_packet] no exist user socket user: " << user->second.get_id() << std::endl;
//         users_.erase(user);
//         return false;
//     }

//     if(!sock->second.send_all(data, len))
//         return false;

//     std::cout << "[debug][send_packet] success" << std::endl; 
    
//     return true;
// }