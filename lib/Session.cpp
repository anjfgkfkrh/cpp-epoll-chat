#include "Session.h"
#include <cstring>


Session::Session(Socket&& sock) : sock_(std::move(sock)) {  }

Session::~Session() { }


bool Session::on_readable() {
    char temp[4096];

    while(true) {
        ssize_t n = sock_.recv(temp, sizeof(temp));

        if(n > 0) {
            recv_buffer_.emplace_back(temp, n);
        }
        else if(n == 0) {
            // 연결 종료
            disconnect();
            return false;
        }
        else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return true;  // 이번 이벤트에서 읽을 수 있는 모든 데이터 읽음

            disconnect(); // 실제 오류
            return false;
        }
    }
}


bool Session::extract_packet(Packet& packet) {
    int offset = 0;

    if(recv_buffer_.size() < sizeof(Header))
        return false;

    Header header;
    std::memcpy(&packet.header, recv_buffer_.data() + offset, sizeof(Header));
    offset += sizeof(Header);


    if(recv_buffer_.size() < offset + sizeof(Request))
        return false;
    
    Request request;
    std::memcpy(&packet.request, recv_buffer_.data() + offset, sizeof(Request));
    offset += sizeof(Request);


    if(request.body_len > 0) {
        if(recv_buffer_.size() < offset + sizeof(Header))
            return false;
        
        packet.body.resize(request.body_len);
        std::memcpy(packet.body.data(), recv_buffer_.data() + offset, sizeof(request.body_len));
    }

    packet.header = std::move(header);
    packet.request = std::move(request);

    return true;
}


void Session::disconnect() {
    sock_.release();
}