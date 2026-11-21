// El otro extremo?

#include "mv_server.hpp"

#include <array>
#include <cmath>

#include <switch.h>

namespace {

constexpr float MasterVolumeDefault = 1.0f;
constexpr std::array<float, 5> MasterVolumeSteps = {
    1.0f, 1.5f, 2.0f, 2.5f, 3.0f
};
constexpr auto ConfigDirPath = "/config/Khowel";
constexpr auto ConfigFilePath = "/config/Khowel/config.bin";

bool audctlInitialized = false;
bool volumeLocked = true;
float masterVolume = MasterVolumeDefault;
float volumeBeforeLock = MasterVolumeDefault;

float stepToVolume(std::uint8_t step) {
    const auto lastStep = static_cast<std::uint8_t>(MasterVolumeSteps.size() - 1);
    return MasterVolumeSteps[step <= lastStep ? step : 0];
}

std::uint8_t volumeToStep(float volume) {
    if (!std::isfinite(volume))
        return 0;

    std::uint8_t nearest = 0;
    float nearestDistance = std::abs(volume - MasterVolumeSteps[0]);
    for (std::uint8_t i = 1; i < MasterVolumeSteps.size(); ++i) {
        const float distance = std::abs(volume - MasterVolumeSteps[i]);
        if (distance < nearestDistance) {
            nearest = i;
            nearestDistance = distance;
        }
    }
    return nearest;
}

FsFileSystem* getSdCardFileSystem() {
    return fsdevGetDeviceFileSystem("sdmc");
}

bool readSavedVolume(const char* path, float& value) {
    auto* fs = getSdCardFileSystem();
    if (!fs)
        return false;

    FsFile file;
    if (R_FAILED(fsFsOpenFile(fs, path, FsOpenMode_Read, &file)))
        return false;

    u64 bytesRead = 0;
    const Result readResult = fsFileRead(
        &file, 0, &value, sizeof(value), FsReadOption_None, &bytesRead
    );
    fsFileClose(&file);

    return R_SUCCEEDED(readResult) && bytesRead == sizeof(value) &&
           std::isfinite(value);
}

float loadSavedVolume(bool& loaded) {
    float value = MasterVolumeDefault;
    loaded = false;

    if (readSavedVolume(ConfigFilePath, value)) {
        loaded = true;
        return value;
    }

    return loaded ? value : MasterVolumeDefault;
}

void persistVolume(float value) {
    auto* fs = getSdCardFileSystem();
    if (!fs)
        return;

    Result rc = fsFsCreateDirectory(fs, ConfigDirPath);
    if (R_FAILED(rc) && R_DESCRIPTION(rc) != 2)
        return;

    rc = fsFsCreateFile(fs, ConfigFilePath, sizeof(value), 0);
    if (R_FAILED(rc) && R_DESCRIPTION(rc) != 2)
        return;

    FsFile file;
    if (R_FAILED(fsFsOpenFile(fs, ConfigFilePath, FsOpenMode_Write, &file)))
        return;

    if (R_SUCCEEDED(fsFileSetSize(&file, sizeof(value)))) {
        fsFileWrite(&file, 0, &value, sizeof(value), FsWriteOption_None);
        fsFileFlush(&file);
    }
    fsFileClose(&file);
}

bool isBlockedTarget(AudioTarget target) {
    return target == AudioTarget_Headphone ||
           target == AudioTarget_Tv ||
           target == AudioTarget_UsbOutputDevice ||
           target == AudioTarget_Bluetooth;
}

bool isCurrentTargetBlocked() {
    if (!audctlInitialized)
        return true;

    AudioTarget target = AudioTarget_Invalid;
    if (R_FAILED(audctlGetActiveOutputTarget(&target)))
        return true;

    return isBlockedTarget(target);
}

void applyMasterVolume() {
    if (audctlInitialized)
        audctlSetSystemOutputMasterVolume(masterVolume);
}

} // namespace

namespace mv {

bool initialize() {
    if (audctlInitialized)
        return true;

    if (R_FAILED(audctlInitialize()))
        return false;
    audctlInitialized = true;

    bool hasSavedVolume = false;
    masterVolume = loadSavedVolume(hasSavedVolume);
    if (!hasSavedVolume && R_FAILED(audctlGetSystemOutputMasterVolume(&masterVolume)))
        masterVolume = MasterVolumeDefault;

    masterVolume = stepToVolume(volumeToStep(masterVolume));
    volumeBeforeLock = masterVolume;
    volumeLocked = isCurrentTargetBlocked();
    if (volumeLocked)
        masterVolume = MasterVolumeDefault;

    applyMasterVolume();
    return true;
}

void shutdown() {
    if (!audctlInitialized)
        return;

    persistVolume(volumeLocked ? volumeBeforeLock : masterVolume);
    audctlExit();
    audctlInitialized = false;
}

State getState() {
    return {
        volumeToStep(audctlInitialized && volumeLocked ? volumeBeforeLock : masterVolume),
        static_cast<std::uint8_t>(!audctlInitialized || volumeLocked),
        static_cast<std::uint8_t>(audctlInitialized),
        0
    };
}

bool isInitialized() {
    return audctlInitialized;
}

void refresh() {
    if (!audctlInitialized)
        return;

    const bool shouldLock = isCurrentTargetBlocked();
    if (shouldLock == volumeLocked)
        return;

    volumeLocked = shouldLock;
    if (volumeLocked) {
        volumeBeforeLock = masterVolume;
        masterVolume = MasterVolumeDefault;
    } else {
        masterVolume = stepToVolume(volumeToStep(volumeBeforeLock));
    }

    applyMasterVolume();
}

void setStep(std::uint8_t step) {
    if (!audctlInitialized || volumeLocked)
        return;

    masterVolume = stepToVolume(step);
    volumeBeforeLock = masterVolume;
    applyMasterVolume();
    persistVolume(masterVolume);
}

void reset() {
    setStep(0);
}

} // namespace mv
