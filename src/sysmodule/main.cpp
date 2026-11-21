#include <cstdint>
#include <cstring>

#include <switch.h>
#include <switch/runtime/hosversion.h>
#include <switch/services/set.h>
#include <switch/sf/cmif.h>
#include <switch/sf/hipc.h>

#include "services/mv_server.hpp"

namespace {

constexpr std::size_t InnerHeapSize = 0x80000;
constexpr u64 AudioRefreshIntervalNs = 250'000'000;
constexpr u64 RetryIntervalNs = 500'000'000;
constexpr s32 MaximumClientSessions = 1;

Handle servicePort = INVALID_HANDLE;
Handle clientSession = INVALID_HANDLE;
Handle replyTarget = INVALID_HANDLE;

void replyToClient(
    Result result, u32 token = 0, const void* payload = nullptr,
    std::size_t payloadSize = 0
) {
    void* tls = armGetTls();
    const HipcRequest response = hipcMakeRequestInline(
        tls, .type = CmifCommandType_Invalid, .num_data_words = 16
    );

    const auto alignedData = cmifGetAlignedDataStart(response.data_words, tls);

    auto* hipcHeader = static_cast<HipcHeader*>(tls);
    const auto* responseEnd = static_cast<const u8*>(alignedData) +
        sizeof(CmifOutHeader) + payloadSize;
    const std::size_t responseWords = (
        responseEnd - reinterpret_cast<const u8*>(response.data_words) +
        sizeof(u32) - 1
    ) / sizeof(u32);
    hipcHeader->num_data_words = static_cast<u32>(responseWords);

    auto* outHeader = static_cast<CmifOutHeader*>(alignedData);
    *outHeader = CmifOutHeader{
        .magic = CMIF_OUT_HEADER_MAGIC,
        .version = 0,
        .result = result,
        .token = token,
    };
    if (payloadSize != 0)
        std::memcpy(outHeader + 1, payload, payloadSize);
}

void handleRequest() {
    void* tls = armGetTls();
    const HipcParsedRequest request = hipcParseRequest(tls);
    if (request.meta.type != CmifCommandType_Request &&
        request.meta.type != CmifCommandType_RequestWithContext) {
        replyToClient(MAKERESULT(Module_Libnx, LibnxError_BadInput));
        return;
    }

    auto* inHeader = static_cast<CmifInHeader*>(
        cmifGetAlignedDataStart(request.data.data_words, tls)
    );
    if (inHeader->magic != CMIF_IN_HEADER_MAGIC) {
        replyToClient(MAKERESULT(Module_Libnx, LibnxError_BadInput));
        return;
    }

    const u32 responseToken = inHeader->token;
    const auto* payload = reinterpret_cast<const u8*>(inHeader + 1);
    const auto* requestEnd = reinterpret_cast<const u8*>(request.data.data_words) +
        request.meta.num_data_words * sizeof(u32);
    Result result = 0;

    switch (static_cast<mv::ipc::Command>(inHeader->command_id)) {
        case mv::ipc::Command::GetState: {
            const auto state = mv::getState();
            replyToClient(0, responseToken, &state, sizeof(state));
            return;
        }
        case mv::ipc::Command::SetStep: {
            if (payload + sizeof(std::uint8_t) > requestEnd) {
                result = MAKERESULT(Module_Libnx, LibnxError_BadInput);
                break;
            }
            const auto step = payload[0];
            if (step > 4) {
                result = MAKERESULT(Module_Libnx, LibnxError_BadInput);
                break;
            }
            if (!mv::isInitialized()) {
                result = MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
                break;
            }
            mv::setStep(step);
            break;
        }
        case mv::ipc::Command::Reset:
            if (!mv::isInitialized()) {
                result = MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
                break;
            }
            mv::reset();
            break;
        default:
            result = MAKERESULT(Module_Libnx, LibnxError_BadInput);
            break;
    }

    replyToClient(result, responseToken);
}

void closeClientSession() {
    if (clientSession == INVALID_HANDLE)
        return;
    svcCloseHandle(clientSession);
    clientSession = INVALID_HANDLE;
}

void serviceLoop() {
    while (true) {
        const Handle handles[] = {
            servicePort,
            clientSession,
        };
        const s32 handleCount = clientSession == INVALID_HANDLE ? 1 : 2;
        s32 index = -1;
        const Result rc = svcReplyAndReceive(
            &index, handles, handleCount, replyTarget, AudioRefreshIntervalNs
        );
        replyTarget = INVALID_HANDLE;

        if (rc == KERNELRESULT(TimedOut)) {
            if (mv::isInitialized())
                mv::refresh();
            else
                mv::initialize();
            continue;
        }

        if (R_FAILED(rc)) {
            if (clientSession != INVALID_HANDLE)
                closeClientSession();
            else
                svcSleepThread(RetryIntervalNs);
            continue;
        }

        if (index == 0) {
            if (clientSession == INVALID_HANDLE &&
                R_FAILED(svcAcceptSession(&clientSession, servicePort)))
                clientSession = INVALID_HANDLE;
            continue;
        }

        if (index == 1 && clientSession != INVALID_HANDLE) {
            handleRequest();
            replyTarget = clientSession;
        }
    }
}

}

extern "C" {

u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

void __libnx_initheap() {
    static u8 innerHeap[InnerHeapSize];
    extern void* fake_heap_start;
    extern void* fake_heap_end;
    fake_heap_start = innerHeap;
    fake_heap_end = innerHeap + sizeof(innerHeap);
}

void __appInit() {
    Result rc = smInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    if (hosversionGet() == 0) {
        rc = setsysInitialize();
        if (R_SUCCEEDED(rc)) {
            SetSysFirmwareVersion firmwareVersion{};
            rc = setsysGetFirmwareVersion(&firmwareVersion);
            if (R_SUCCEEDED(rc)) {
                hosversionSet(MAKEHOSVERSION(
                    firmwareVersion.major,
                    firmwareVersion.minor,
                    firmwareVersion.micro
                ));
            }
            setsysExit();
        }
    }
    rc = fsInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));

    fsdevMountSdmc();
}

void __appExit() {
    fsdevUnmountAll();
    fsExit();
    smExit();
}

}

int main() {
    Result rc = smRegisterServiceCmif(
        &servicePort, smEncodeName(mv::ipc::ServiceName), false,
        MaximumClientSessions
    );
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    mv::initialize();
    serviceLoop();
    return 0;
}
