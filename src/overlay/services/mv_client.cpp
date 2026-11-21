// Uno de los extremos del servicio mastervolume, dios mio que dolor de cabeza me dio este servicio,
// pero me costo hacer que funcionara :v

#include "mv_client.hpp"

#include <switch.h>

namespace {

Service mvService{};
bool serviceConnected = false;
std::uint32_t lastClientError = 0;

bool ensureConnected() {
    if (serviceConnected)
        return true;

    const Result rc = smGetService(&mvService, mv::ipc::ServiceName);
    serviceConnected = R_SUCCEEDED(rc);
    lastClientError = serviceConnected ? 0 : static_cast<std::uint32_t>(rc);
    return serviceConnected;
}

void disconnect() {
    if (!serviceConnected)
        return;

    serviceClose(&mvService);
    serviceConnected = false;
}

}
namespace mv::client {

bool initialize() {
    return ensureConnected();
}

std::uint32_t lastError() {
    return lastClientError;
}

void shutdown() {
    disconnect();
}

bool getState(ipc::State& state) {
    if (!ensureConnected())
        return false;

    const Result rc = serviceDispatchOut(
        &mvService, static_cast<std::uint32_t>(ipc::Command::GetState), state
    );
    if (R_FAILED(rc)) {
        lastClientError = static_cast<std::uint32_t>(rc);
        disconnect();
        return false;
    }
    lastClientError = 0;
    return true;
}

bool setStep(std::uint8_t step) {
    if (step > 4 || !ensureConnected())
        return false;

    const Result rc = serviceDispatchIn(
        &mvService, static_cast<std::uint32_t>(ipc::Command::SetStep), step
    );
    if (R_FAILED(rc)) {
        lastClientError = static_cast<std::uint32_t>(rc);
        disconnect();
        return false;
    }
    lastClientError = 0;
    return true;
}

bool reset() {
    if (!ensureConnected())
        return false;

    const Result rc = serviceDispatch(
        &mvService, static_cast<std::uint32_t>(ipc::Command::Reset)
    );
    if (R_FAILED(rc)) {
        lastClientError = static_cast<std::uint32_t>(rc);
        disconnect();
        return false;
    }
    lastClientError = 0;
    return true;
}

} // namespace mv::client
