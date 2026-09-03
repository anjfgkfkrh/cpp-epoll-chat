# epoll-chat-server

epoll 기반 이벤트 드리븐 논블로킹 채팅 서버 — C++20, 메시지 패싱 멀티스레드 아키텍처, PostgreSQL

## 구조

```
                            TCP 클라이언트
                                  │
 ═════════════════════════════════│═════════════════════════════════════
  Main Thread                     ▼
                        ┌──────────────────────┐
                        │        Server        │  epoll 루프 · accept / recv
                        └──┬────────────────┬──┘
                  Packet   │                │  AuthEvent
                           ▼                │
                ┌──────────────────────┐    │
                │      RoomManager     │    │   RoomEvent 생성 + 라우팅
                └──────────┬───────────┘    │
 ═══════════════════════════│═══════════════│═════════════════════════
                RoomEvent   │               │
              ┌─────────────┴──┐            │
              ▼                ▼            ▼
       ┌────────────┐   ┌────────────┐   ┌────────────┐
       │ RoomWorker │ ··│ RoomWorker │   │ AuthWorker │ ·· 각자 전용 스레드
       │   방 소유    │   │ (1개는 로비) │   │ Argon2 검증 │
       └──────┬─────┘   └──────┬─────┘   └──────┬─────┘
              └────────────────┴────────────────┘
                                  │ ▲
                            DBJob │ │ callback(event)
                                  ▼ │
                        ┌──────────────────────┐
                        │       DBWorker       │  전용 스레드 · 블로킹 쿼리 격리
                        └──────────┬───────────┘
                                   ▼
                            PostgreSQL (libpq)
```

워커는 클라이언트 소켓에 **직접** 응답을 씁니다. `EPOLLOUT` 전환이나 세션 종료처럼
epoll 조작이 필요할 때만 `IServerService` → `eventfd`로 메인 스레드에 위임합니다.

| 클래스 | 실행 스레드 | 역할 | 상태 |
|------|------|------|------|
| `Server` | Main | epoll 루프, accept/recv. `SessionManager` · `UserManager` · `RoomManager` · `AuthWorker` · `DBWorker` 소유 | ✅ |
| `RoomManager` | 호출 스레드 (Main + Worker) | `RoomWorker[]` 소유. 패킷을 `RoomEvent`로 만들어 담당 워커에게 라우팅 | ✅ |
| `RoomWorker` | 전용 | 방을 소유하고 `RoomEvent`의 Action을 순차 실행. `WorkerType::Lobby`인 워커 하나가 로비를 전담 | ✅ |
| `AuthWorker` | 전용 (2~4개) | 비밀번호 해시 생성·검증(Argon2id). CPU 바운드라 코어 수에 맞춰 확장 | 🚧 |
| `DBWorker` | 전용 (1개) | 모든 DB 작업 전담. `DBJob` 큐로 요청 수신 | ✅ |

### 로비

접속한 유저는 방에 들어가기 전까지 **로비**(`LOBBY_ROOM_ID`)에 머뭅니다.
로비는 전용 `RoomWorker`(`LOBBY_WORKER_ID`)가 전담하므로, 로비 인원이 많아져도
일반 방의 처리가 느려지지 않습니다. 방 입장·퇴장은 항상 로비를 거칩니다.

### RoomEvent — 실행할 Action의 레시피

`RoomManager`는 패킷을 받으면 곧바로 처리하지 않고, **무엇을 순서대로 해야 하는지**를 `RoomEvent`에 적어 워커에게 넘깁니다.

```
CMD_SEND_MESSAGE  →  [ SEND_MESSAGE ]

CMD_JOIN_ROOM     →  [ JOIN_ROOM(대상 방),
                       REQUEST_LEAVE_ROOM(로비),   ← 로비 워커로 재라우팅
                       LEAVE_ROOM(로비),
                       SEND_RESPONSE ]
```

`RoomWorker`는 이 Action 리스트를 앞에서부터 하나씩 실행합니다. `REQUEST_*` Action처럼
다른 워커가 소유한 방을 건드려야 하는 경우, 이벤트를 그대로 해당 워커에게 넘기고 자신은 즉시 반환합니다.
이벤트는 `current_action`이 가리키는 **중단된 지점부터** 다음 워커에서 이어서 처리되므로,
워커가 서로의 상태를 직접 만질 일이 없습니다.

### DBJob — 콜백으로 이어붙이는 비동기 DB

`RoomWorker`와 `AuthWorker`는 DB가 필요하면 `DBJob`에 **콜백(`std::function`)과 처리 중이던 이벤트**를 함께 담아
`DBWorker` 큐에 넣고 곧바로 다음 일을 합니다. `DBWorker`는 쿼리 결과를 이벤트에 반영한 뒤 콜백을 실행해,
그 이벤트를 원래 워커의 큐에 다시 밀어 넣습니다.

```
RoomWorker ──DBJob{ event, callback }──▶ DBWorker ──쿼리──▶ PostgreSQL
     ▲                                       │
     └────────── callback(event) ────────────┘
```

쿼리 결과(`DBResult`)는 `DBWorker` 안에서 도메인 데이터로 변환되어 `event.data`에 실립니다.
`PGresult`가 워커 계층까지 흘러가지 않으므로, 룸 로직은 libpq에 의존하지 않습니다.

어느 스레드도 쿼리를 기다리며 멈추지 않고, 이벤트는 중단된 지점부터 이어서 처리됩니다.

### 설계 원칙

핵심은 하나입니다. **이벤트 루프 스레드는 절대 무제한 대기하지 않는다.**

- 소켓 I/O는 논블로킹 + epoll로 다중화
- 스레드 간 상태 공유 대신 이벤트 큐로 메시지 전달 (Actor 유사)
- mutex는 큐 경계에서만 짧게 사용
- 블로킹이 불가피한 DB 작업은 `DBWorker` 전용 스레드로 격리, 결과는 콜백으로 회신
- CPU를 오래 점유하는 해시 검증은 `AuthWorker`로 분리
- epoll 등록 변경·세션 종료 요청은 `eventfd`로 메인 스레드에 위임

## 기술 스택

C++20 · Linux epoll · POSIX 스레드 · PostgreSQL (libpq) · 자체 바이너리 프로토콜

## 빌드

```bash
make            # 서버 + 클라이언트 (release)
make debug      # 디버그 빌드
make server     # 서버만
make clean
```

의존성: `g++` (C++20), `libpq-dev`, PostgreSQL 15+

## 실행

### 최초 1회 — DB 준비

```bash
# 1. DB와 전용 계정 생성
cd /tmp && sudo -u postgres psql <<'SQL'
CREATE USER chatapp WITH PASSWORD '원하는_비밀번호';
CREATE DATABASE chatdb OWNER chatapp ENCODING 'UTF8';
SQL

# 2. 비밀번호는 ~/.pgpass 에 (libpq가 자동으로 읽음, 권한 600 필수)
echo "127.0.0.1:5432:chatdb:chatapp:원하는_비밀번호" >> ~/.pgpass
chmod 600 ~/.pgpass

# 3. 접속 정보 설정 (비밀번호는 넣지 않음)
cp .env.example .env    # PGHOST/PGPORT/PGUSER/PGDATABASE 채우기

# 4. 스키마 생성
set -a; . ./.env; set +a
psql -v ON_ERROR_STOP=1 -1 -f server/src/sql/schema.sql
```

`ON_ERROR_STOP=1`과 `-1`(단일 트랜잭션)이 있어야 중간 실패 시 전부 롤백됩니다.

### 서버 · 클라이언트

```bash
make run                # 서버 실행 (.env 자동 주입, 기본 포트 8080)
./out/bin/run_client    # 클라이언트 실행
```

`make run` 없이 직접 실행할 때는 환경 변수를 먼저 주입해야 합니다.
libpq는 `.env` 파일을 읽지 않고 **환경 변수**만 봅니다.

```bash
set -a; . ./.env; set +a
./out/bin/run_server
```

## 프로토콜

길이 기반 바이너리 패킷.

| 타입 | 방향 | 용도 |
|-----|-----|-----|
| `REQUEST` | 클라이언트 → 서버 | 방 생성 / 참여 / 퇴장 / 메시지 전송 / 과거 메시지 요청 |
| `RESPONSE` | 서버 → 클라이언트 | 요청 처리 결과 |
| `BROADCAST` | 서버 → 클라이언트 | 채팅 메시지, 유저 입퇴장 알림 |

바이트 구조, 명령별 요청·응답 규격, 상태 코드는 **[PROTOCOL.md](PROTOCOL.md)** 참고.
