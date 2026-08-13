#pragma once

enum class EventState: uint8_t {
    Pending,
    Processing,
    Finish,
};