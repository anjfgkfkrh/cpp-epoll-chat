#include "Session.h"
#include <cstring>

using Protocol::Header;
using Protocol::Request;

Session::Session(Socket&& sock) : sock_(std::move(sock)), disconnecting_(false) {  }

Session::~Session() { }

bool Session::on_readable() {
    char temp[4096];

    while(true) {
        ssize_t n = sock_.recv(temp, sizeof(temp));

        if(n > 0) {
            recv_buffer_.insert(recv_buffer_.end(), temp, temp + n);
        }
        else if(n == 0) {
            // 연결 종료
            mark_disconnecting();
            return false;
        }
        else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return true;  // 이번 이벤트에서 읽을 수 있는 모든 데이터 읽음
            if (errno == EINTR)
                continue;

            mark_disconnecting(); // 실제 오류
            return false;
        }
    }
}


bool Session::extract_packet(Packet& packet) {
    size_t offset = 0;

    if(recv_buffer_.size() < sizeof(Header))
        return false;
    std::memcpy(&packet.header, recv_buffer_.data() + offset, sizeof(Header));
    offset += sizeof(Header);


    if(recv_buffer_.size() < offset + sizeof(Request))
        return false;
    std::memcpy(&packet.request, recv_buffer_.data() + offset, sizeof(Request));
    offset += sizeof(Request);

    // if(packet.request.body_len > MAX_BODY_LEN) { mark_disconnecting(); return false; }
    if(packet.request.body_len > 0) {
        if(recv_buffer_.size() < offset + sizeof(Header))
            return false;
        
        packet.body.resize(packet.request.body_len);
        std::memcpy(packet.body.data(), recv_buffer_.data() + offset, sizeof(packet.request.body_len));
    }

    // 사용된 데이터 삭제
    recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + offset);

    return true;
}

void Session::pack(std::vector<std::byte> data) {
    send_queue_.emplace(std::move(data), 0);
}

Session::FlushResult Session::flush() {
    ssize_t n = 0;

    while(!send_queue_.empty()){
        SendBuffer& buffer = send_queue_.front();

        while(buffer.offset < buffer.buffer.size()){
            n = sock_.send(reinterpret_cast<char*>(buffer.buffer.data() + buffer.offset), buffer.buffer.size() - buffer.offset);

            if(n > 0)
                buffer.offset += n;
            else if(n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
                return FlushResult::Pending;    // 지금은 더 보낼 수 없음
            else if(n == -1 && errno == EINTR)
                continue;
            else    // 실제 오류
                return FlushResult::Error;
        }

        send_queue_.pop();
    }

    return FlushResult::Complete;
}