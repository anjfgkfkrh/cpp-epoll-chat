#include "Room.h"
#include <algorithm>

Room::Room(int room_id, int max_clients) : room_id_(room_id), max_clients_(max_clients), current_clients_(0) {}
Room::~Room() {}

bool Room::join_client(uint16_t user_id) {
    // 정원 체크
    if(current_clients_ >= max_clients_)
        return false;

    users_.emplace_back(user_id);
    current_clients_++;

    return true;
}

void Room::exit_client(uint16_t user_id) {
    auto itr = std::find_if(users_.begin(), users_.end(), [user_id](uint16_t id) { return id == user_id; });

    if(itr != users_.end()){
        users_.erase(itr);
        current_clients_--;
    }
}

bool Room::find_client(uint16_t user_id) {
    auto itr = std::find_if(users_.begin(), users_.end(), [user_id](uint16_t id) { return id == user_id; });

    if(itr != users_.end())
        return true;
    else
        return false;
}
std::vector<uint16_t>& Room::clients() { return users_; }

// void Room::send_message(uint16_t sender, const std::string& message) {
//     // 패킷 조립
//     Broadcast packet {
//         .event = EVT_MESSAGE,
//         .sender_id = sender, // 임시(fd)
//         .room_id = static_cast<uint16_t>(room_id_),
//         .body_len = message.size()
//     };


//     for(auto& user : clients_){
//         if(user == sender)
//             continue;
//         send_packet(user, PKT_BROADCAST, &packet, sizeof(Broadcast), message);
//     }
// }

// void Room::send_packet(uint16_t user_id, uint16_t type, const void* packet, size_t packet_len, const std::string& body) {
//     Header header;
//     header.type = type;

//     client.pack(&header, sizeof(Header));
//     client.pack(packet, sizeof(packet_len));
//     if(body.size() > 0)
//         client.pack(body.data(), body.size());
//     client.flush();
// }

int Room::get_clients_num() { return current_clients_; }