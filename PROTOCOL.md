# 채팅 프로토콜

길이 기반 바이너리 프로토콜. 요청 5종과 그 응답, 서버가 먼저 보내는 브로드캐스트의 바이트 구조를 정리한다.

정의 원본: [Protocol.h](server/src/protocol/Protocol.h) · [Result.h](server/src/core/Result.h)

---

## 1. 패킷 프레이밍

모든 패킷은 `Header` 뒤에 종류별 구조체가 오고, 그 뒤에 `body_len` 만큼의 본문이 붙는다.

```
[ Header ][ Request | Response | Broadcast ][ body ]
```

구조체는 `memcpy` 로 그대로 직렬화되므로 **정렬 패딩까지 바이트 스트림에 포함된다.**
다른 언어로 클라이언트를 구현할 때 가장 어긋나기 쉬운 지점이다.

### Header — 2바이트 (모든 패킷 공통)

| 오프셋 | 필드 | 타입 | 값 |
|---|---|---|---|
| 0–1 | `type` | uint16 | 1=REQUEST, 2=RESPONSE, 3=BROADCAST |

### Request — 12바이트 (클라이언트 → 서버)

| 오프셋 | 필드 | 타입 |
|---|---|---|
| 0–1 | `command` | uint16 |
| 2–3 | *(패딩)* | — |
| 4–7 | `room_id` | uint32 |
| 8–11 | `body_len` | uint32 |
| 12– | `body` | body_len 바이트 |

### Response — 8바이트 (서버 → 요청자)

| 오프셋 | 필드 | 타입 |
|---|---|---|
| 0–1 | `command` | uint16 |
| 2 | `status` | uint8 |
| 3 | *(패딩)* | — |
| 4–7 | `body_len` | uint32 |
| 8– | `body` | body_len 바이트 |

### Broadcast — 12바이트 (서버 → 방 전원)

| 오프셋 | 필드 | 타입 |
|---|---|---|
| 0–1 | `event` | uint16 |
| 2–3 | `sender_id` | uint16 |
| 4–7 | `room_id` | uint32 |
| 8–11 | `body_len` | uint32 |
| 12– | `body` | body_len 바이트 |

> 바이트 순서는 **호스트 엔디안**을 그대로 쓴다. 서버와 클라이언트가 같은 아키텍처일 때만 안전하다.

---

## 2. 요청과 응답

요청 하나당 응답 하나가 돌아온다. 응답의 `command` 는 요청한 값을 그대로 되돌려주므로 짝을 맞출 수 있다.
**실패한 응답의 body 는 항상 0바이트다.**

### `CMD_CREATE_ROOM` (command = 1)

방을 만들고 곧바로 입장한다.

| | |
|---|---|
| **요청** `room_id` | 무시됨 — 번호는 서버가 발급 |
| **요청** `body` | 없음 |
| **응답** `status` | `Success` / `DBError` |
| **응답** `body` | **8바이트** — 발급된 room_id (int64) |

### `CMD_JOIN_ROOM` (command = 2)

방에 입장하며 최근 대화를 함께 받는다.

| | |
|---|---|
| **요청** `room_id` | 들어갈 방 번호 |
| **요청** `body` | 없음 |
| **응답** `status` | `Success` / `RoomNotFound` / `RoomFull` |
| **응답** `body` | **이전 메시지 목록** — 최신 50건 (§4) |

### `CMD_LEAVE_ROOM` (command = 3)

방에서 나와 로비로 돌아간다.

| | |
|---|---|
| **요청** `room_id` | 현재 방 번호 |
| **요청** `body` | 없음 |
| **응답** `status` | `Success` / `RoomNotFound` |
| **응답** `body` | 없음 |

### `CMD_SEND_MESSAGE` (command = 4)

방 전원에게 메시지를 보낸다.

| | |
|---|---|
| **요청** `room_id` | 현재 방 번호 |
| **요청** `body` | 메시지 본문 (UTF-8, 길이 접두사 없음) |
| **응답** `status` | `Success` / `RoomNotFound` |
| **응답** `body` | 없음 |

> 같은 방의 **다른** 참여자에게는 별도로 `BROADCAST` 가 나간다.
> 보낸 사람은 브로드캐스트를 받지 않고 응답만 받는다.

### `CMD_LOAD_MESSAGE` (command = 5)

더 오래된 메시지를 이어서 가져온다.

| | |
|---|---|
| **요청** `room_id` | 현재 방 번호 |
| **요청** `body` | **8바이트** — 커서 (int64, 지금까지 받은 가장 오래된 id) |
| **응답** `status` | `Success` / `RoomNotFound` / `DBError` |
| **응답** `body` | **이전 메시지 목록** — 커서보다 오래된 50건 (§4) |

> 커서가 **0 · 음수 · 8바이트 미만**이면 모두 "최신부터"로 처리된다. 첫 요청은 body 를 비워 보내도 된다.
> 결과가 0건이면 `Success` + body 4바이트(개수 0)가 오며, 이것이 *더 이상 없음*을 뜻한다.

---

## 3. 응답 상태 코드

| 값 | 이름 | 의미 |
|---|---|---|
| 0 | `None` | 결과 코드가 설정되지 않음. 정상 흐름에서는 나오지 않는다 |
| 1 | `Success` | 성공 |
| 2 | `RoomNotFound` | 해당 번호의 방이 메모리에 없음 |
| 3 | `RoomAlreadyExists` | 이미 존재하는 방 번호 |
| 4 | `RoomFull` | 방 정원 초과 |
| 5 | `UserNotFound` | 방에서 해당 유저를 찾을 수 없음 |
| 6 | `DBNotFound` | DB 조회 결과가 없음 |
| 7 | `DBError` | DB 연결 끊김·타임아웃 등 |
| 8 | `DBDuplicate` | 고유 제약 위반 |

---

## 4. 이전 메시지 body 포맷

`CMD_JOIN_ROOM` 과 `CMD_LOAD_MESSAGE` 의 응답 body 에 쓰인다.

**최신순(내림차순)** 이므로 마지막 원소가 가장 오래된 메시지이며,
그 `id` 가 다음 `CMD_LOAD_MESSAGE` 요청의 커서가 된다.

### 전체

| 오프셋 | 필드 | 타입 |
|---|---|---|
| 0–3 | `count` | uint32 |
| 4– | 메시지 × count | 아래 구조 반복 |

### 메시지 하나

| 오프셋 | 필드 | 타입 |
|---|---|---|
| +0 | `id` | int64 |
| +8 | `sender_id` | int64 |
| +16 | `created_epoch` | int64 |
| +24 | `nick_len` | uint32 |
| +28 | `nickname` | nick_len 바이트 |
| … | `content_len` | uint32 |
| … | `content` | content_len 바이트 |

- `created_epoch` 는 1970-01-01 UTC 기준 **초** 단위 정수다. 시간대 변환과 표시 형식은 클라이언트가 정한다.
- ⚠️ 문자열 길이는 클라이언트가 정하지 않은 값이므로, **읽기 전에 남은 버퍼 길이를 반드시 검사**해야 한다.

구현 참고: 서버 [Repository.cpp](server/src/db/Repository.cpp) `db::message::serialize()`,
클라이언트 [MessageCodec.h](client/src/MessageCodec.h)

---

## 5. 브로드캐스트

요청과 무관하게 서버가 먼저 보낸다. 응답을 기다리는 중에도 도착할 수 있으므로,
수신 루프는 `Header.type` 을 먼저 보고 분기해야 한다.

| event | 이름 | body | 의미 |
|---|---|---|---|
| 1 | `EVT_MESSAGE` | 메시지 본문 (UTF-8) | 같은 방의 다른 사람이 보낸 메시지 |
| 2 | `EVT_USER_JOIN` | 없음 | 유저 입장 |
| 3 | `EVT_USER_LEAVE` | 없음 | 유저 퇴장 |

> `room_id` 가 함께 오므로, 방을 옮기는 순간 도착한 이전 방의 브로드캐스트는 걸러낼 수 있다.

---

## 6. 방 번호 규칙

| 번호 | 용도 | 비고 |
|---|---|---|
| 1 | 로비 | 접속하면 자동으로 들어가 있다. 직접 create·join 하지 않는다 |
| 2–99 | 예약 | 고정 번호가 필요한 방을 위해 비워 둔 구간 |
| 100– | 일반 방 | DB 시퀀스가 순차 발급. 클라이언트는 번호를 지정할 수 없다 |

번호 규칙은 [schema.sql](server/src/sql/schema.sql) 의 `ALTER SEQUENCE room_room_id_seq RESTART WITH 100` 에서 정해진다.

> 방은 **모든 참여자가 나가면 메모리에서 사라진다.** DB에는 행이 남지만
> 그 번호로 다시 `JOIN` 하면 `RoomNotFound` 가 돌아온다.
