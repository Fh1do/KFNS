#pragma once

#include "../../ipc/protocol.hpp"

namespace mv::client {

bool initialize();
std::uint32_t lastError();
void shutdown();
bool getState(ipc::State& state);
bool setStep(std::uint8_t step);
bool reset();

}