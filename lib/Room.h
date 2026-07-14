#include <vector>
#include <string>
#include <functional>
#include "Protocol.h"

// 룸 정보를 관리하는 데이터 컨테이너 클래스
class Room {
private:
    int room_id_;
    std::vector<uint16_t> users_;
    int max_clients_;
    int current_clients_ = 0;
    // std::function<bool(uint16_t user_id, void* data, size_t len)> send_fn;
public:
    Room(int room_id, int max_clients=5);
    ~Room();

    // 복사 방지
    Room(Room&&) = default;
    Room& operator=(Room&&) = default;

    bool join_client(uint16_t user_id);              // 클라이언트 방 입장
    void exit_client(uint16_t user_id);              // 클라이언트 방 퇴장
    bool find_client(uint16_t user_id);              // 클라이언트 찾기
    std::vector<uint16_t>& clients();                // 클라이언트들 반환
    // void send_message(uint16_t sender, const std::string& message); // 모든 클라이언트들에게 메시지 전송
    int get_clients_num();

private:
    // void send_packet(uint16_t user_id, uint16_t type, const void* packet, size_t packet_len, const std::string& body);    // 패킷 조립 및 전송
};