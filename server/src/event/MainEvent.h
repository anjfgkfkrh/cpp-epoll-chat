#pragma once

enum class MainEventType {
    Disconnect,
    EnableEpollOut,
    DisalbeEpollOut
};

struct MainEvent {
    MainEventType type;
    int fd;
};

