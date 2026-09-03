#include "RoomWorker.h"
#include "RoomManager.h"
#include "MessageRepository.h"
#include "Codec.h"

using Protocol::Header;
using Protocol::Request;
using Protocol::Broadcast;
using Protocol::Response;
using Protocol::serialize_packet;
using Protocol::Command;
using Protocol::PacketType;

RoomWorker::RoomWorker(uint16_t worker_id, RoomManager& manager, IServerService& sservice, UserManager& user_manager, DBWorker& db, WorkerType type):
    worker_id_(worker_id), manager_(manager), sservice_(sservice), user_manager_(user_manager), db_(db), running_(false), type_(type) {
        if(type_ == WorkerType::Lobby) {
            rooms_.try_emplace(LOBBY_ROOM_ID, LOBBY_ROOM_ID, MAX_LOBBY_SESSIONS);
        }
    }

void RoomWorker::start() {
    running_ = true;

    thread_ = std::thread(&RoomWorker::thread_main, this);
}

void RoomWorker::stop() {
    {
        std::lock_guard lock(mutex_);
        
        running_ = false;

        cv_.notify_one();
    }

    if(thread_.joinable())
        thread_.join();
}

void RoomWorker::post_event(RoomEvent&& event) {
    {
        std::lock_guard lock(mutex_);

        event_queue_.emplace(std::move(event));
    }

    cv_.notify_one();
}

void RoomWorker::thread_main() {
    while(running_){
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this]
            {
                return !running_ || !event_queue_.empty(); // 깨어나기 전 조건 확인 -> true 반환시 깨어남
            });

        while(!event_queue_.empty()) {
            auto event = std::move(event_queue_.front());
            event_queue_.pop();

            lock.unlock();

            process_event(std::move(event));

            lock.lock();
        }
    }
}

void RoomWorker::process_event(RoomEvent&& event) {
    while(event.current_action < event.action_len) {
        if(event.result_code != Result::Success && event.result_code != Result::None) {  // db 작업 실패 시
            send_response(event);
            return;
        }

        auto action = event.actions[event.current_action++];

        switch(action.command) {
        case ActionCommand::REQUEST_DB_CREATE_ROOM:
            request_db_create_room(event);
            return;
        case ActionCommand::CREATE_ROOM:
            event.result_code = create_room(event);
            break;
        case ActionCommand::JOIN_ROOM:
            event.result_code = join_room(event);
            break;
        case ActionCommand::LEAVE_ROOM:
            event.result_code = leave_room(event);
            break;
        case ActionCommand::REQUEST_JOIN_ROOM:
            event.target_room_id = action.target_room_id;
            manager_.route_event(std::move(event));
            return;
        case ActionCommand::REQUEST_LEAVE_ROOM:
            event.target_room_id = action.target_room_id;
            manager_.route_event(std::move(event));
            return;
        case ActionCommand::SEND_MESSAGE:
            event.result_code = send_message(event);
            break;
        case ActionCommand::REQUEST_DB_LOAD_MESSAGE:
            request_db_load_message(event);
            return;
        case ActionCommand::SAVE_MESSAGE:
            save_message(event);
            break;
        case ActionCommand::SEND_RESPONSE:
            send_response(event);
            return;
        case ActionCommand::FLUSH:
            flush(event.session);
            return;
        case ActionCommand::DISCONNECT_SESSION:
            diconnect_session(event.fd, event.target_room_id);
            return;
        }

        // Action 실패시 response 바로 전송 후 이벤트 폐기
        if(event.result_code != Result::Success && action.command != ActionCommand::SEND_RESPONSE) {
            if(event.actions[0].command == ActionCommand::CREATE_ROOM){ // create room 실패 시 해야할 행동
                manager_.erase_room_to_worker(event.target_room_id);
            }

            send_response(event);
            return;
        }
    }
}

void RoomWorker::request_db_create_room(RoomEvent& event) {
    DBJob dbjob;
    dbjob.type = DBJobType::CreateRoom;
    dbjob.on_result = [event = std::move(event), this] (DBStatus status, std::vector<std::byte>&& data) mutable {
        db_create_room_on_result(std::move(event), status, std::move(data));
    };

    db_.post_job(std::move(dbjob));
}

Result RoomWorker::create_room(RoomEvent& event, int max_session) {
    RoomId room_id = event.actions[event.current_action-1].target_room_id;
    auto itr = rooms_.find(room_id);
    if(itr != rooms_.end()) // 이럴리가 없음, 이미 RoomManager에서 room_to_worker로 없는거 확인 하고 옴
        return Result::RoomAlreadyExists;

    rooms_.try_emplace(room_id, room_id, max_session);

    return Result::Success;
}

Result RoomWorker::join_room(RoomEvent& event) {
    return join_room(event.session, event.actions[event.current_action-1].target_room_id);
}

Result RoomWorker::join_room(std::shared_ptr<Session> session, RoomId room_id) {
    // room 조회
    auto itr = rooms_.find(room_id);
    if(itr == rooms_.end())
        return Result::RoomNotFound;

    // join 시도
    Room& room = itr->second;
    if(!room.join_session(session))
        return Result::RoomFull;

    // user의 current_room 변경
    std::shared_ptr<User> user = user_manager_.get_user(session->get_fd());
    if (user == nullptr)
        return Result::UserNotFound;
    user->set_current_room(room_id);
    
    return Result::Success;
}


Result RoomWorker::leave_room(RoomEvent& event) {
    return leave_room(event.session->get_fd(), event.actions[event.current_action-1].target_room_id);
}

Result RoomWorker::leave_room(int fd, RoomId room_id) {
    // 룸 조회
    auto itr = rooms_.find(room_id);
    if(itr == rooms_.end())
        return Result::RoomNotFound;

    // 룸에서 session 삭제
    Room& room = itr->second;
    if(!room.exit_session(fd))
        return Result::UserNotFound;

    // 룸에 아무도 없으면 삭제
    if(room.get_sessions_num() <= 0 && type_ == WorkerType::Room){
        manager_.erase_room_to_worker(room.get_room_id());
        rooms_.erase(room.get_room_id());
    }

    return Result::Success;
}

Result RoomWorker::send_message(RoomEvent& event) {
    auto itr = rooms_.find(event.actions[event.current_action-1].target_room_id);
    if(itr == rooms_.end())
        return Result::RoomNotFound;
    
    Header header;
    header.type = PacketType::PKT_BROADCAST;
    Broadcast broad;
    broad.event = Protocol::Event::EVT_MESSAGE;
    broad.room_id = event.actions[event.current_action-1].target_room_id;
    broad.sender_id = user_manager_.get_user_id(event.session->get_fd());
    broad.body_len = event.request_data.size();

    auto data = serialize_packet<Broadcast>(header, broad, event.request_data);
    
    Room& room = itr->second;

    for(const auto& s : room.get_sessions()){
        std::shared_ptr<User> user;

        if(s.second->get_fd() == event.session->get_fd()) // 송신자에게는 전송 X
            continue;
        
        user = user_manager_.get_user(s.second->get_fd());
        if (user == nullptr || user->get_current_room() != broad.room_id) // 방 소속이 아닐 경우 메시지 전송 X (방 이동 순간의 안전장치)
            continue;

        send_packet(s.second, data);
    }

    return Result::Success;
}

void RoomWorker::send_response(RoomEvent& event) {
    Header header;
    header.type = PacketType::PKT_RESPONSE;

    Response res;
    res.command = event.initial_command;
    res.status = result_to_resresultcode(event.result_code);
    res.body_len = event.response_data.size();

    send_packet(event.session, serialize_packet(header, res, event.response_data));
}

void RoomWorker::save_message(RoomEvent& event) {
    std::string tempnick = "tempnickname";
    std::string content(reinterpret_cast<const char*>(event.request_data.data()), event.request_data.size());

    DBJob dbjob;
    dbjob.type = DBJobType::SaveMessage;
    dbjob.params = db::message::params_for_save(
        event.actions[event.current_action-1].target_room_id, 
        user_manager_.get_user_id(event.session->get_fd()),
         tempnick,
         content);

    db_.post_job(std::move(dbjob));
}

void RoomWorker::request_db_load_message(RoomEvent& event) {
    RoomId room_id = event.actions[event.current_action-1].target_room_id;
    int64_t lasted_message_id = 0;
    if(event.request_data.size() >= sizeof(int64_t))
        std::memcpy(&lasted_message_id, event.request_data.data(), sizeof(int64_t));
    if(lasted_message_id <= 0)
        lasted_message_id = std::numeric_limits<int64_t>::max();

    DBJob dbjob;
    dbjob.type = DBJobType::LoadHistory;
    dbjob.params.emplace_back(std::to_string(room_id));
    dbjob.params.emplace_back(std::to_string(lasted_message_id));
    dbjob.on_result = [event = std::move(event), this](DBStatus status, std::vector<std::byte>&& data) mutable {
        db_load_messages_on_result(std::move(event), status, std::move(data));
    };

    db_.post_job(std::move(dbjob));
}

void RoomWorker::db_create_room_on_result(RoomEvent&& event, DBStatus status, std::vector<std::byte>&& data) {
    db::codec::Reader r{data.data(), data.size()};

    RoomId room_id = 0;

    if(status == DBStatus::Error || !r.take(&room_id, sizeof(room_id))){
        event.result_code = Result::DBError;
        this->manager_.route_event(std::move(event), LOBBY_WORKER_ID);
        return;
    }

    event.result_code = Result::Success;
    event.resolve_room_id(room_id);
    event.response_data = std::move(data);
    uint16_t worker_id = this->manager_.bind_room_worker(event.target_room_id);
    this->manager_.route_event(std::move(event), worker_id);
}

void RoomWorker::db_load_messages_on_result(RoomEvent&& event, DBStatus status, std::vector<std::byte>&& data) {
    if(status == DBStatus::Success){
        event.result_code = Result::Success;
        event.response_data = data;
    }
    else if(status == DBStatus::NotFound)
        event.result_code = Result::DBNotFound;
    else if(status == DBStatus::Error)
        event.result_code = Result::DBError;
    else if(status == DBStatus::Duplicate)
        event.result_code = Result::DBDuplicate;

    event.target_room_id = event.actions[event.current_action-1].target_room_id;
    manager_.route_event(std::move(event));
}

void RoomWorker::flush(std::shared_ptr<Session> session) {
    Session::FlushResult result = session->flush();

    switch(result) {
    case Session::FlushResult::Pending:
        break;
    case Session::FlushResult::Error:
        sservice_.request_close_session(session->get_fd());
        break;
    case Session::FlushResult::Complete:
        sservice_.request_disable_epollout(session->get_fd());
        break;
    }
}

void RoomWorker::send_packet(const std::shared_ptr<Session>& session, const std::vector<std::byte>& data) {
    session->pack(data);
    Session::FlushResult result = session->flush();

    switch(result) 
    {
    case Session::FlushResult::Pending:
        sservice_.request_enable_epollout(session->get_fd());
        break;
    case Session::FlushResult::Error:
        sservice_.request_close_session(session->get_fd());
        break;
    case Session::FlushResult::Complete:
        break;
    }
}



bool RoomWorker::diconnect_session(int fd, RoomId room_id) {
    if(leave_room(fd, room_id) == Result::Success){  // room에 존재하면 바로 퇴장
        return true;
    }
    else{                       // 선택한 room에 존재하지 않으면 소유한 다른 룸들 모두 순회
        for(auto& room : rooms_){
            if(leave_room(fd, room.second.get_room_id()) == Result::Success){
                return true;
            }
        }
    }

    return false;   // 해당 RoomWorker에 session 존재하지않음
}