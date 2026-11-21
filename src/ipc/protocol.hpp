// Mi señor chismoso, el que te eschucha y te delata :v

#pragma once

#include <cstdint>

namespace mv::ipc {

inline constexpr char ServiceName[] = "kfns:mv";

enum class Command : std::uint32_t {
    GetState = 0,
    SetStep = 1,
    Reset = 2,
};

struct State {
    std::uint8_t step;
    std::uint8_t locked;
    std::uint8_t available;
    std::uint8_t reserved;
};

static_assert(sizeof(State) == 4);

}