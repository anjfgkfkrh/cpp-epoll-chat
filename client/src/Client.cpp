#include "Client.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <arpa/inet.h>

using Protocol::Header;
using Protocol::Request;
using Protocol::Response;
using Protocol::Broadcast;
using Protocol::Command;
using Protocol::PacketType;

namespace {

const char* status_text(ResponseResultCode s) {
    switch (s) {
    case ResponseResultCode::Success:           return "성공";
    case ResponseResultCode::RoomNotFound:      return "존재하지 않는 방입니다";
    case ResponseResultCode::RoomAlreadyExists: return "이미 존재하는 방 번호입니다";
    case ResponseResultCode::RoomFull:          return "방이 가득 찼습니다";
    case ResponseResultCode::UserNotFound:      return "해당 방에서 유저를 찾을 수 없습니다";
    case ResponseResultCode::DBNotFound:        return "데이터를 찾을 수 없습니다";
    case ResponseResultCode::DBError:           return "데이터베이스 오류가 발생했습니다";
    case ResponseResultCode::DBDuplicate:       return "이미 존재하는 데이터입니다";
    case ResponseResultCode::None:              return "처리 결과 없음";
    }
    return "알 수 없는 결과";
}

const char* command_text(Command c) {
    switch (c) {
    case Command::CMD_CREATE_ROOM:  return "방 생성";
    case Command::CMD_JOIN_ROOM:    return "방 입장";
    case Command::CMD_LEAVE_ROOM:   return "방 퇴장";
    case Command::CMD_SEND_MESSAGE: return "메시지 전송";
    case Command::CMD_LOAD_MESSAGE: return "과거 메시지 요청";
    }
    return "알 수 없는 명령";
}

} // namespace


Client::Client(uint16_t port) : state_(ClientState::LOBBY), room_id_(0), oldest_message_id_(0), running_(false) {
    setup_command();

    int try_num = 1;
    bool connected = false;
    while (try_num <= 3) {
        std::cout << "[info] 서버 접속 시도 " << try_num << "/3" << std::endl;
        if (connect_server(port)) { connected = true; break; }
        std::cout << "[error] 접속 실패" << std::endl;
        try_num++;
    }
    if (!connected) return;

    std::cout << "[info] 서버 접속 성공 (port " << port << ")" << std::endl;

    events_.resize(MAX_EVENTS);
    epoll_.add(sock_->get(), EPOLLIN);
    epoll_.add(STDIN_FILENO, EPOLLIN);
}

Client::~Client() {}

bool Client::is_connected() const { return sock_.has_value(); }

bool Client::connect_server(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return false; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 내부 테스트용
    addr.sin_port = htons(port);

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        ::close(fd);
        return false;
    }

    sock_ = Socket(fd);
    return true;
}

bool Client::validate_room_id(RoomId room_id) const {
    if (room_id == 0) {
        std::cout << "[error] 방 번호는 1 이상이어야 합니다" << std::endl;
        return false;
    }
    if (room_id == LOBBY_ROOM_ID) {
        std::cout << "[error] " << LOBBY_ROOM_ID
                  << "번은 로비 전용 번호라 직접 입장하거나 만들 수 없습니다."
                  << " 접속하면 자동으로 로비에 들어가 있습니다." << std::endl;
        return false;
    }
    return true;
}

bool Client::setup_command() {
    commands_lobby_["/create"] = [this](std::istringstream&) {
        // 방 번호는 서버가 DB 시퀀스로 발급하므로 클라이언트는 지정하지 않는다.
        // room_id 자리에는 0을 넣고, 발급된 번호는 응답 body 로 받는다.
        if (!send_request(Command::CMD_CREATE_ROOM, 0))
            std::cout << "[error] 요청 전송 실패" << std::endl;
    };

    commands_lobby_["/join"] = [this](std::istringstream& args) {
        RoomId room_id;
        if (!(args >> room_id)) {
            std::cout << "[error] 사용법: /join [room_id]" << std::endl;
            return;
        }
        if (!validate_room_id(room_id))
            return;
        if (!send_request(Command::CMD_JOIN_ROOM, room_id))
            std::cout << "[error] 요청 전송 실패" << std::endl;
    };

    commands_lobby_["/exit"] = [this](std::istringstream&) {
        std::cout << "[info] 종료합니다" << std::endl;
        running_ = false;
    };

    commands_room_["/load"] = [this](std::istringstream&) {
        // 커서: 지금까지 받은 가장 오래된 메시지 id. 0 이면 서버가 최신부터 보내준다.
        std::string body(sizeof(int64_t), '\0');
        std::memcpy(body.data(), &oldest_message_id_, sizeof(int64_t));
        if (!send_request(Command::CMD_LOAD_MESSAGE, room_id_, body))
            std::cout << "[error] 요청 전송 실패" << std::endl;
    };

    commands_room_["/leave"] = [this](std::istringstream&) {
        if (!send_request(Command::CMD_LEAVE_ROOM, room_id_))
            std::cout << "[error] 요청 전송 실패" << std::endl;
    };

    commands_room_["/exit"] = [this](std::istringstream&) {
        std::cout << "[info] 종료합니다" << std::endl;
        running_ = false;
    };

    return true;
}

int Client::print_history(const std::string& body) {
    std::vector<MessageCodec::Message> msgs;
    if (!MessageCodec::parse(body, msgs))
        return -1;
    if (msgs.empty())
        return 0;

    // 서버는 최신순(DESC)으로 보내므로 화면에는 오래된 것부터 거꾸로 출력한다.
    std::cout << "──── 이전 메시지 " << msgs.size() << "건 ────" << std::endl;
    for (auto itr = msgs.rbegin(); itr != msgs.rend(); ++itr) {
        std::time_t t = static_cast<std::time_t>(itr->created_epoch);
        std::tm tm{};
        localtime_r(&t, &tm);
        char ts[16];
        std::strftime(ts, sizeof(ts), "%H:%M", &tm);
        std::cout << "  [" << ts << "] " << itr->sender_nick << ": " << itr->content << std::endl;
    }
    std::cout << "────────────────────" << std::endl;

    // 다음 /load 커서: 받은 것 중 가장 오래된 id (DESC 이므로 마지막 원소)
    oldest_message_id_ = msgs.back().id;
    return static_cast<int>(msgs.size());
}

void Client::print_prompt() const {
    if (state_ == ClientState::LOBBY)
        std::cout << "[로비] /create | /join [번호] | /exit" << std::endl;
    else
        std::cout << "[방 " << room_id_ << "] 메시지 입력 | /load | /leave | /exit" << std::endl;
}

void Client::run() {
    if (!sock_) {
        std::cout << "[error] 서버에 접속하지 못해 실행할 수 없습니다" << std::endl;
        return;
    }

    running_ = true;
    print_prompt();

    while (running_) {
        int n = epoll_.wait(events_);

        for (int i = 0; i < n && running_; i++) {
            int fd = events_[i].data.fd;

            if (fd == STDIN_FILENO) {
                handle_input();
            }
            else if (fd == sock_->get()) {
                if (!handle_packet()) {
                    std::cout << "[info] 서버와의 연결이 끊어졌습니다" << std::endl;
                    running_ = false;
                }
            }
        }
    }
}

std::string Client::read_line() {
    char buf[1024];
    ssize_t bytes = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (bytes <= 0) return "";

    // 개행 제거
    while (bytes > 0 && (buf[bytes - 1] == '\n' || buf[bytes - 1] == '\r'))
        bytes--;

    return std::string(buf, bytes);
}

bool Client::send_request(Command cmd, RoomId room_id, const std::string& body) {
    Header header{};
    header.type = PacketType::PKT_REQUEST;

    Request req{};
    req.command  = cmd;
    req.room_id  = room_id;
    req.body_len = static_cast<uint32_t>(body.size());

    sock_->pack(&header, sizeof(Header));
    sock_->pack(&req, sizeof(Request));
    if (!body.empty())
        sock_->pack(body.data(), body.size());

    if (!sock_->flush())
        return false;

    pending_ = Pending{cmd, room_id};
    return true;
}

bool Client::handle_input() {
    // 입력은 항상 소비해야 한다. 소비하지 않으면 level-trigger epoll 이 계속 깨어난다.
    std::string input = read_line();

    if (pending_) {
        std::cout << "[info] 이전 요청(" << command_text(pending_->command)
                  << ")의 응답을 기다리는 중입니다" << std::endl;
        return false;
    }

    if (input.empty())
        return true;

    std::istringstream stream(input);
    std::string cmd;
    stream >> cmd;

    if (state_ == ClientState::LOBBY) {
        auto itr = commands_lobby_.find(cmd);
        if (itr != commands_lobby_.end())
            itr->second(stream);
        else
            std::cout << "[error] 로비에서는 /create, /join, /exit 만 사용할 수 있습니다" << std::endl;
    }
    else {
        auto itr = commands_room_.find(cmd);
        if (itr != commands_room_.end())
            itr->second(stream);
        else if (!send_request(Command::CMD_SEND_MESSAGE, room_id_, input))
            std::cout << "[error] 메시지 전송 실패" << std::endl;
    }

    return true;
}

bool Client::handle_packet() {
    Header header{};
    if (!sock_->recv_all(&header, sizeof(Header)))
        return false;

    switch (header.type) {
    case PacketType::PKT_RESPONSE:  return handle_response();
    case PacketType::PKT_BROADCAST: return handle_broadcast();
    default:
        std::cout << "[error] 알 수 없는 패킷 종류: "
                  << static_cast<int>(header.type) << " — 연결을 종료합니다" << std::endl;
        return false;   // 스트림 정렬이 깨졌으므로 복구 불가
    }
}

bool Client::handle_response() {
    Response res{};
    if (!sock_->recv_all(&res, sizeof(Response)))
        return false;

    // 본문은 결과와 무관하게 항상 끝까지 읽어야 스트림 정렬이 유지된다.
    std::string body(res.body_len, '\0');
    if (res.body_len > 0 && !sock_->recv_all(body.data(), res.body_len))
        return false;

    if (!pending_) {
        std::cout << "[error] 요청하지 않은 응답을 받았습니다" << std::endl;
        return true;
    }
    if (pending_->command != res.command) {
        std::cout << "[error] 응답 명령 불일치 (요청=" << command_text(pending_->command)
                  << ", 응답=" << command_text(res.command) << ")" << std::endl;
        pending_.reset();
        return true;
    }

    const RoomId requested_room = pending_->room_id;
    const Command  cmd            = pending_->command;
    pending_.reset();

    if (res.status != ResponseResultCode::Success) {
        std::cout << "[실패] " << command_text(cmd) << ": " << status_text(res.status) << std::endl;
        print_prompt();
        return true;
    }

    // 응답에는 room_id 가 실려오지 않으므로 요청 시 기억해 둔 값을 사용한다.
    switch (cmd) {
    case Command::CMD_CREATE_ROOM: {
        // 방 번호는 서버(DB)가 발급하며 응답 body 에 int64 8바이트로 실려온다.
        if (body.size() < sizeof(int64_t)) {
            std::cout << "[error] 서버가 방 번호를 보내지 않았습니다" << std::endl;
            break;
        }
        int64_t created = 0;
        std::memcpy(&created, body.data(), sizeof(created));
        room_id_ = created;
        state_   = ClientState::IN_ROOM;
        oldest_message_id_ = 0;         // 새 방이므로 커서 초기화
        std::cout << "[성공] 방 " << room_id_ << " 을(를) 만들고 입장했습니다" << std::endl;
        break;
    }

    case Command::CMD_JOIN_ROOM:
        room_id_ = requested_room;
        state_   = ClientState::IN_ROOM;
        oldest_message_id_ = 0;         // 방이 바뀌었으므로 커서 초기화
        std::cout << "[성공] 방 " << room_id_ << " 에 입장했습니다" << std::endl;
        // 서버는 입장 응답 body 에 최근 메시지를 함께 실어 보낸다.
        if (print_history(body) < 0)
            std::cout << "[error] 이전 메시지를 해석하지 못했습니다" << std::endl;
        break;

    case Command::CMD_LEAVE_ROOM:
        std::cout << "[성공] 방 " << room_id_ << " 에서 나왔습니다" << std::endl;
        room_id_ = 0;
        oldest_message_id_ = 0;
        state_   = ClientState::LOBBY;
        break;

    case Command::CMD_SEND_MESSAGE:
        break;      // 전송 성공은 따로 출력하지 않는다

    case Command::CMD_LOAD_MESSAGE: {
        int n = print_history(body);
        if (n < 0)
            std::cout << "[error] 과거 메시지 응답을 해석하지 못했습니다" << std::endl;
        else if (n == 0)
            std::cout << "[info] 더 이상 이전 메시지가 없습니다" << std::endl;
        break;
    }
    }

    print_prompt();
    return true;
}

bool Client::handle_broadcast() {
    Broadcast broad{};
    if (!sock_->recv_all(&broad, sizeof(Broadcast)))
        return false;

    std::string body(broad.body_len, '\0');
    if (broad.body_len > 0 && !sock_->recv_all(body.data(), broad.body_len))
        return false;

    // 내가 속한 방의 것이 아니면 무시 (스트림은 이미 정상적으로 소비했다)
    if (state_ != ClientState::IN_ROOM || broad.room_id != room_id_)
        return true;

    switch (broad.event) {
    case Protocol::Event::EVT_MESSAGE:
        std::cout << "  유저" << broad.sender_id << ": " << body << std::endl;
        break;
    case Protocol::Event::EVT_USER_JOIN:
        std::cout << "  * 유저" << broad.sender_id << " 님이 입장했습니다" << std::endl;
        break;
    case Protocol::Event::EVT_USER_LEAVE:
        std::cout << "  * 유저" << broad.sender_id << " 님이 퇴장했습니다" << std::endl;
        break;
    }

    return true;
}
