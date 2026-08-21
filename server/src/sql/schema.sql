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


-- 메시지 (월별 파티셔닝)
CREATE TABLE messages (
    id          BIGSERIAL,
    room_id     BIGINT      NOT NULL,
    sender_id   BIGINT      NOT NULL,
    sender_nick VARCHAR(32) NOT NULL,
    content     TEXT        NOT NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (id, created_at)
)   PARTITION BY RANGE (created_at);





CREATE TABLE messages_2026_08 PARTITION OF messages
    FOR VALUES FROM ('2026-08-01') TO ('2026-09-01');
CREATE TABLE messages_2026_09 PARTITION OF messages
    FOR VALUES FROM ('2026-09-01') TO ('2026-10-01');
CREATE TABLE messages_default PARTITION OF messages DEFAULT;

CREATE INDEX ON messages (room_id, id DESC);