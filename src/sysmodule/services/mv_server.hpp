#pragma once

#include <cstdint>

#include "../../ipc/protocol.hpp"

namespace mv {

using State = ipc::State;

bool initialize();
void shutdown();
State getState();
bool isInitialized();
void refresh();
void setStep(std::uint8_t step);
void reset();

}