#pragma once

#include "RoomWorker.h"
#include "Protocol.h"
#include "Session.h"
#include "RoomEvent.h"
#include "DBWorker.h"

#include <unordered_map>
#include <shared_mutex>
#include <memory>

constexpr uint16_t LOBBY_WORKER_ID = 1; // 로비 전용 RoomWorker ID
constexpr uint32_t LOBBY_ROOM_ID = 1;   // 로비 Room ID

/*
RoomWoker를 관리하는 클래스
들어온 패킷을 알맞은 RoomWorker을 찾아 Packet 전달
룸 생성,패킷 라우팅
*/
class RoomManager {
private:
    std::unordered_map<uint16_t, std::unique_ptr<RoomWorker>> room_workers_;  // worker_id to RoomWorker
    std::unordered_map<uint32_t, uint16_t> room_to_worker_;
    UserManager& user_manager_;
    IServerService& sservice_;

    std::shared_mutex room_to_worker_mutex_;
    const int room_worker_num_;

public:
    RoomManager(UserManager& user_manager, DBWorker& db, IServerService& sservice);
    ~RoomManager();

    void start();
    void stop();

    void dispatch_packet(Packet&& packet, std::shared_ptr<Session> session); // 새로운 패킷 받아서 RoomEvent 생성 후 post_event()로 라우팅  // Main Thread에서 호출
    Result route_event(RoomEvent&& room_event, uint16_t target_worker_id = 0); // RoomEvent를 RoomWorker에게 라우팅   // Main, Worker Thread에서 호출

    uint16_t bind_room_worker(uint32_t room_id);
    void erase_room_to_worker(uint32_t room_id);
    void disconnect_session(int fd, uint32_t room_id);

    void request_flush(std::shared_ptr<Session> session);

private:
    uint16_t find_worker(uint32_t room_id);

    void plan_create_room(RoomEvent& event);
    void plan_join_room(RoomEvent& event);
    void plan_leave_room(RoomEvent& event);
    void plan_send_message(RoomEvent& event);
    void plan_load_message(RoomEvent& event);
};