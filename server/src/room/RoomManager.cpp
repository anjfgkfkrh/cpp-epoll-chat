#include "RoomManager.h"
#include "RoomEvent.h"

#include <thread>
#include <random>


RoomManager::RoomManager(UserManager& user_manager, IServerService& sservice): 
    user_manager_(user_manager), sservice_(sservice), room_worker_num_(std::thread::hardware_concurrency()) {     
    // 로비 전용 RoomWorker 생성
    room_workers_.emplace(LOBBY_WORKER_ID, std::make_unique<RoomWorker>(LOBBY_ROOM_ID, *this, sservice_, user_manager_, RoomWorker::WorkerType::Lobby));
    room_to_worker_.emplace(LOBBY_ROOM_ID, LOBBY_WORKER_ID);

    // Room 전용 RoomWorker 생성, worker_id = 2 ~ room_worker_num
    for(int i=0; i<room_worker_num_ - 1; i++) {
        room_workers_.emplace(i+2, std::make_unique<RoomWorker>(i+2, *this, sservice_, user_manager_));
    }
}

RoomManager::~RoomManager() { }

void RoomManager::start() {
    // 실행
    for(auto& worker : room_workers_) {
        worker.second->start();
    }
}

void RoomManager::stop() {
    for(auto& worker : room_workers_) {
        worker.second->stop();
    }
}



void RoomManager::dispatch_packet(Packet&& packet, std::shared_ptr<Session> session) {
    // Server(Main Thread)가 받은 Packet을 RoomEvent로 조립 및 Action을 plan하여 RoomWorker로 넘기는 함수
    // 1. RoomEvent 생성
    // 2. Action plan
    // 3. RoomWorker로 route

    // RoomEvent 생성
    RoomEvent room_event;
    room_event.initial_command = packet.request.command;
    room_event.data = packet.body;
    room_event.session = session;
    room_event.target_room_id = packet.request.room_id;

    uint16_t worker_id = 0;

    // Action plan
    switch(room_event.initial_command) {
    case Protocol::Command::CMD_CREATE_ROOM:
        plan_create_room(room_event);
        break;
    case Protocol::Command::CMD_JOIN_ROOM:
        plan_join_room(room_event);
        break;
    case Protocol::Command::CMD_LEAVE_ROOM:
        plan_leave_room(room_event);
        break;
    case Protocol::Command::CMD_SEND_MESSAGE:
        plan_send_message(room_event);
        break;
    }
    
    route_event(std::move(room_event), worker_id);
}



Result RoomManager::route_event(RoomEvent&& room_event, uint16_t target_worker_id) {
    // Server 또는 다른 RoomWorker가 RoomWorker에게 RoomEvent를 라우팅하는 함수

    // 지정된 worker_id가 없으면 worker_id를 찾아서 반환
    uint16_t worker_id = (target_worker_id == 0) ? find_worker(room_event.target_room_id) : target_worker_id;
    if(worker_id == 0) // 방이 존재하지않음
        return Result::RoomNotFound;

    room_workers_.at(worker_id)->post_event(std::move(room_event));

    return Result::Success;
}



void RoomManager::erase_room_to_worker(uint32_t room_id) {
    {
        std::lock_guard lock(room_to_worker_mutex_);

        room_to_worker_.erase(room_id);
    }
}



void RoomManager::disconnect_session(int fd, uint32_t room_id) {
    RoomEvent event;
    event.fd = fd;
    event.target_room_id = room_id;
    event.add_action(ActionCommand::DISCONNECT_SESSION, event.target_room_id);

    route_event(std::move(event));
}



uint16_t RoomManager::find_worker(uint32_t room_id) {
    {   
        std::shared_lock lock(room_to_worker_mutex_);

        if(room_to_worker_.find(room_id) == room_to_worker_.end())
            return 0;
        
        return room_to_worker_.at(room_id);
    }
}



uint16_t RoomManager::bind_room_worker(uint32_t room_id) {
    // 2개의 RoomWorker 랜덤 추첨
    // 2개 중 room의 갯수가 적은 RoomWorker에 바인드

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dist(LOBBY_WORKER_ID+1, room_worker_num_); // 1은 로비라 제외

    uint16_t first_worker = dist(gen);

    uint16_t second_worker;
    do{
        second_worker = dist(gen);
    }
    while(first_worker == second_worker && room_worker_num_ >= 3);

    uint16_t worker_id = (room_workers_.at(first_worker)->get_rooms_num() < room_workers_.at(second_worker)->get_rooms_num()) ? first_worker : second_worker;

    {
        std::lock_guard lock(room_to_worker_mutex_);

        room_to_worker_.emplace(room_id, worker_id);
    }

    return worker_id;
}

void RoomManager::request_flush(std::shared_ptr<Session> session) {
    std::shared_ptr<User> user = user_manager_.get_user(session->get_fd());
    if(user == nullptr)
        return;
    
    RoomEvent event;
    event.session = session;
    event.target_room_id = user->get_current_room();
    event.add_action(ActionCommand::FLUSH);

    route_event(std::move(event));
}

void RoomManager::plan_create_room(RoomEvent& event) {
    // 방 중복 검사
    if(find_worker(event.target_room_id) != 0){
        event.add_action(ActionCommand::SEND_RESPONSE, LOBBY_ROOM_ID);
        event.result_code = Result::RoomAlreadyExists;
        event.target_room_id = LOBBY_ROOM_ID;
    }


    // room_id를 미리 worker에 바인드
    uint16_t worker_id = bind_room_worker(event.target_room_id);

    event.add_action(ActionCommand::CREATE_ROOM, event.target_room_id);
    event.add_action(ActionCommand::JOIN_ROOM, event.target_room_id);
    event.add_action(ActionCommand::REQUEST_LEAVE_ROOM, LOBBY_ROOM_ID);
    event.add_action(ActionCommand::LEAVE_ROOM, LOBBY_ROOM_ID);
    event.add_action(ActionCommand::SEND_RESPONSE);
}

void RoomManager::plan_join_room(RoomEvent& event) {
    event.add_action(ActionCommand::JOIN_ROOM, event.target_room_id);
    event.add_action(ActionCommand::REQUEST_LEAVE_ROOM, LOBBY_ROOM_ID);
    event.add_action(ActionCommand::LEAVE_ROOM, LOBBY_ROOM_ID);
    event.add_action(ActionCommand::SEND_RESPONSE);
}

void RoomManager::plan_leave_room(RoomEvent& event) {
    event.add_action(ActionCommand::JOIN_ROOM, LOBBY_ROOM_ID);
    event.add_action(ActionCommand::REQUEST_LEAVE_ROOM, event.target_room_id);
    event.add_action(ActionCommand::LEAVE_ROOM, event.target_room_id);
    event.add_action(ActionCommand::SEND_RESPONSE);
    event.target_room_id = LOBBY_ROOM_ID;
}

void RoomManager::plan_send_message(RoomEvent& event) {
    event.add_action(ActionCommand::SEND_MESSAGE, event.target_room_id);
    event.add_action(ActionCommand::SEND_RESPONSE);
}