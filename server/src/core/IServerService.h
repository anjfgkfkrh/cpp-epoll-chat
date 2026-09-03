#pragma once

class IServerService {
public:
    virtual void request_enable_epollout(int fd) = 0;
    virtual void request_disable_epollout(int fd) = 0;
    virtual void request_close_session(int fd) = 0;

    virtual ~IServerService() = default;
};