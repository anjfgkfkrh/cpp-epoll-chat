DROP TABLE IF EXISTS account, room, messages CASCADE;

-- 계정
CREATE TABLE account (
    user_id     BIGSERIAL   PRIMARY KEY,
    login_id    VARCHAR(32) NOT NULL UNIQUE,
    pass_hash   TEXT        NOT NULL,
    nickname    VARCHAR(32) NOT NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);


-- 방
CREATE TABLE room (
    room_id     BIGSERIAL   PRIMARY KEY,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- 로비(LOBBY_ROOM_ID = 1)를 미리 넣어둔다.
-- 로비도 messages.room_id 외래키의 참조 대상이라 행이 존재해야 로비 대화가 저장된다.
INSERT INTO room(room_id) VALUES (1);

-- BIGSERIAL 에 값을 직접 넣으면 시퀀스가 따라오지 않아 다음 발급이 1로 중복된다.
-- 예약 번호(1~99)와 겹치지 않게 일반 방은 100번부터 발급한다.
ALTER SEQUENCE room_room_id_seq RESTART WITH 100;


-- 메시지 (월별 파티셔닝)
CREATE TABLE messages (
    id          BIGSERIAL,
    room_id     BIGINT      NOT NULL,
    sender_id   BIGINT      NOT NULL,
    sender_nick VARCHAR(32) NOT NULL,
    content     TEXT        NOT NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (id, created_at),
    -- FOREIGN KEY (sender_id) REFERENCES account(user_id),
    FOREIGN KEY (room_id) REFERENCES room(room_id)
)   PARTITION BY RANGE (created_at);





CREATE TABLE messages_2026_08 PARTITION OF messages
    FOR VALUES FROM ('2026-08-01') TO ('2026-09-01');
CREATE TABLE messages_2026_09 PARTITION OF messages
    FOR VALUES FROM ('2026-09-01') TO ('2026-10-01');
CREATE TABLE messages_default PARTITION OF messages DEFAULT;

CREATE INDEX ON messages (room_id, id DESC);