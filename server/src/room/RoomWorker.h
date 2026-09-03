#pragma once

#include <unordered_map>
#include <queue>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "Room.h"
#include "Socket.h"
#include "User.h"
#include "Protocol.h"
#include "RoomEvent.h"
#include "IServerService.h"
#include "EventState.h"
#include "UserManager.h"
#include "DBWorker.h"
#include "Type.h"

using Protocol::Packet;
class RoomManager;

constexpr int MAX_LOBBY_SESSIONS = 999; // 로비 최대 접속 가능 인원 수

class RoomWorker {
public:
    enum class WorkerType : uint16_t {
        Room,
        Lobby
    };

private:
    uint16_t worker_id_;
    RoomWorker::WorkerType type_;

    RoomManager& manager_;
    IServerService& sservice_;
    DBWorker& db_;

    std::unordered_map<RoomId, Room> rooms_;
    UserManager& user_manager_;

    std::queue<RoomEvent> event_queue_;
    std::thread thread_;
    std::atomic<bool> running_;
    std::mutex mutex_;
    std::condition_variable cv_;

public:
    RoomWorker(uint16_t worker_id, RoomManager& manager, IServerService& sservice, UserManager& user_manager, DBWorker& db, WorkerType type = WorkerType::Room);

    void start();
    void stop();
    
    void post_event(RoomEvent&& event);

    inline size_t get_rooms_num() { return rooms_.size(); }

    bool diconnect_session(int fd, RoomId room_id);   // 소유한 room들을 순회하여 session 제거

private:
    void thread_main();
    void process_event(RoomEvent&& event);

    Result create_room(RoomEvent& event, int max_session = 5);
    Result join_room(RoomEvent& event);
    Result join_room(std::shared_ptr<Session> session, RoomId room_id);
    Result leave_room(RoomEvent& event);
    Result leave_room(int fd, RoomId room_id);
    Result send_message(RoomEvent& event);
    void send_response(RoomEvent& event);
    void flush(std::shared_ptr<Session> session);

    void send_packet(const std::shared_ptr<Session>& session, const std::vector<std::byte>& data);

    void request_db_create_room(RoomEvent& event);
    void request_db_load_message(RoomEvent& event);
    void save_message(RoomEvent& event);

    void db_load_messages_on_result(RoomEvent&& event, DBStatus status, std::vector<std::byte>&& data);
    void db_create_room_on_result(RoomEvent&& event, DBStatus status, std::vector<std::byte>&& data);
};