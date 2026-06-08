#include "Room.h"
#include <algorithm>


Room::Room(int room_id, int max_clients=5) : room_id_(room_id), max_clients_(max_clients) {}
Room::~Room() {}

bool Room::join_client(Socket&& client) {
    // 정원 체크
    if(current_clients_ >= max_clients_)
        return false;

    clients_.emplace_back(std::move(client));
    current_clients_++;

    return true;
}

std::optional<Socket> Room::exit_client(int fd) {
    auto itr = std::find_if(clients_.begin(), clients_.end(), [fd](const Socket& s) { return s.get() == fd; });

    if(itr != clients_.end()){
        Socket client = std::move(*itr);
        clients_.erase(itr);
        current_clients_--;
        return client;
    }
    return std::nullopt;
}

const std::optional<Socket&> Room::find_client(int fd) {
    auto itr = std::find_if(clients_.begin(), clients_.end(), [fd](const Socket& s) { return s.get() == fd; });

    if(itr != clients_.end())
        return *itr;

    return std::nullopt;
}
const std::vector<Socket>& Room::clients() { return clients_; }

void Room::send_message(Socket& sender, std::string message) {
    // 패킷 조립
    Broadcast packet {
        .event = EVT_MESSAGE,
        .sender_id = sender.get(), // 임시
        .room_id = room_id_,
        .body_len = message.size()
    };
    message.copy(packet.body, message.size());


    for(auto& client : clients_){
        // 송신자 확인
        if(client.get() == sender.get())
            continue;
        // 전송
        write(client.get(), message.data(), message.size());
    }
}