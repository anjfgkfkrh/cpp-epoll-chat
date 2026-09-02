#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include "Result.h"

// 네트워크 통신에 사용되는 프로토콜을 정의한 헤더 파일
namespace Protocol {
    enum class PacketType : uint16_t {
        PKT_REQUEST     = 1,    // Request 패킷
        PKT_RESPONSE    = 2,    // Response 패킷
        PKT_BROADCAST   = 3,    // Broadcast 패킷
    };

    enum class Command : uint16_t {
        CMD_CREATE_ROOM     = 1,    // 룸 생성
        CMD_JOIN_ROOM       = 2,    // 룸 참여
        CMD_LEAVE_ROOM      = 3,    // 룸 퇴장
        CMD_SEND_MESSAGE    = 4,    // 메시지 전송
        CMD_LOAD_MESSAGE    = 5,    // 과거 메시지 요청
    };

    enum class Event : uint16_t {
        EVT_MESSAGE     = 1,    // 채팅 메시지
        EVT_USER_JOIN   = 2,    // 유저 입장
        EVT_USER_LEAVE  = 3,    // 유저 퇴장
    };

    struct Header{
        PacketType type;      // 패킷 종류
    };

    struct Request {
        Command command;   // 기능 번호
        uint32_t room_id;   // 룸 번호
        uint32_t body_len;  // 데이터 길이
    };

    // 패킷을 감싸서 객체간 쉽게 넘기기위한 구조체
    struct Packet {
        Header header;
        Request request;
        std::vector<std::byte> body;
    };

    struct Response {
        Command command;   // 기능 번호
        ResponseResultCode status;    // 처리 결과    0: 성공, 1: 실패
        uint32_t body_len;  // 데이터 길이
    };

    struct Broadcast {
        Event event;     // 이벤트 종류
        uint16_t sender_id; // 송신자
        uint32_t room_id;   // 방 번호
        uint32_t body_len;  // 메시지 길이
    };

    template <typename T>
    std::vector<std::byte> serialize_packet(const Header& header, const T& packet, const std::vector<std::byte>& body = {}) {
        std::vector<std::byte> data;
        data.resize(sizeof(Header) + sizeof(T) + body.size());

        ssize_t offset = 0;
        
        std::memcpy(data.data() + offset , &header, sizeof(Header));
        offset += sizeof(Header);

        std::memcpy(data.data() + offset, &packet, sizeof(T));
        offset += sizeof(T);

        if(!body.empty())
            std::memcpy(data.data() + offset, body.data(), body.size());

        return data;
    }
}