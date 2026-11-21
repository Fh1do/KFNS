// Alguien se tomará el tiempo de leer todo esto? ni idea la verdad, pero bueno,
// me gusta documentar mis proyectos, aunque no se si alguien los lea realmente.

// (Si alguien esta leyendo esto, me gustaria saber su opinion sobre el proyecto, si tiene alguna sugerencia, etc.)

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include <switch.h>

#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include "alphasymbols/volume.hpp"
#include "components/custom_steptrackbar.hpp"
#include "services/mv_client.hpp"

namespace {

class AudioControlGUI : public tsl::Gui {
public:
    constexpr static u32 MasterVolumeAreaHeight = 91;
    constexpr static u32 StatePollInterval = 30;

    AudioControlGUI() {
        mv::ipc::State state{};
        const bool ipcConnected = mv::client::getState(state);
        mv_ipc_error = mv::client::lastError();
        mv_ipc_connected = ipcConnected;
        mv_service_available = ipcConnected && state.available != 0;
        volume_slider_locked = !mv_service_available || state.locked != 0;
        currentStep = state.step;
    }

    tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame(APP_TITLE, std::string(APP_VERSION) + "    Made by " + APP_AUTHOR);

        root_frame = frame;
        rebuildContent();
        return frame;
    }

    void update() override {
        if (++statePollCounter < StatePollInterval)
            return;
        statePollCounter = 0;

        mv::ipc::State state{};
        const bool ipcConnected = mv::client::getState(state);
        const auto ipcError = mv::client::lastError();
        const bool serviceAvailable = ipcConnected && state.available != 0;
        const bool currentLockState = !serviceAvailable || state.locked != 0;
        const bool stateChanged = ipcConnected != mv_ipc_connected ||
                                  ipcError != mv_ipc_error ||
                                  serviceAvailable != mv_service_available ||
                                  currentLockState != volume_slider_locked;

        mv_ipc_connected = ipcConnected;
        mv_ipc_error = ipcError;
        mv_service_available = serviceAvailable;
        volume_slider_locked = currentLockState;
        if (serviceAvailable)
            currentStep = state.step;

        if (stateChanged) {
            this->removeFocus();
            rebuildContent();
            this->requestFocus(root_frame, tsl::FocusDirection::None);
            return;
        }

        if (!volume_slider_locked && mvol_slider &&
            mvol_slider->getProgress() != currentStep)
            mvol_slider->setProgress(currentStep);
    }

    bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos,
                     HidAnalogStickState leftJoyStick,
                     HidAnalogStickState rightJoyStick) override {
        if (!volume_slider_locked && mv_service_available && mvol_slider &&
            this->getFocusedElement() == mvol_slider &&
            (keysDown & HidNpadButton_X)) {
            if (mv::client::reset()) {
                mv::ipc::State state{};
                if (mv::client::getState(state)) {
                    currentStep = state.step;
                    mvol_slider->setProgress(currentStep);
                }
            }
            return true;
        }

        return tsl::Gui::handleInput(
            keysDown, keysHeld, touchPos, leftJoyStick, rightJoyStick
        );
    }

private:
    void rebuildContent() {
        auto* list = new tsl::elm::List();
        mvol_header = new tsl::elm::CategoryHeader("");

        if (volume_slider_locked) {
            mvol_slider = nullptr;
            mvol_header->setText("Master Volume:\n");
            list->addItem(mvol_header);

            auto* lockedText = new tsl::elm::CustomDrawer(
                [ipcConnected = mv_ipc_connected,
                 ipcError = mv_ipc_error,
                 serviceAvailable = mv_service_available](
                    tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h
                ) {
                    const auto color = renderer->a(0xF33F);
                    const auto separatorColor = renderer->a(tsl::separatorColor);
                    renderer->drawRect(x + 7, y, w - 14, 1, separatorColor);
                    renderer->drawRect(x + 7, y + h - 1, w - 14, 1, separatorColor);

                    if (!ipcConnected) {
                        renderer->drawString(
                            "The MV IPC service is not responding.",
                            false, x + 13, y + 33, 14, color
                        );
                        char errorText[40];
                        std::snprintf(errorText, sizeof(errorText), "IPC Result: 0x%08X", ipcError);
                        renderer->drawString(errorText, false, x + 13, y + 53, 14, color);
                        return;
                    }

                    if (!serviceAvailable) {
                        renderer->drawString(
                            "The sysmodule responds, but audctl is",
                            false, x + 13, y + 33, 14, color
                        );
                        renderer->drawString(
                            "unavailable or failed to initialize.",
                            false, x + 13, y + 53, 14, color
                        );
                        return;
                    }

                    renderer->drawString(
                        "This menu is  disabled  for hearing  safety reasons.",
                        false, x + 18, y + 24, 14, color
                    );
                    renderer->drawString(
                        "Master volume cannot be adjusted with this audio",
                        false, x + 18, y + 41, 14, color
                    );
                    renderer->drawString(
                        "output. You  can  only  adjust  the master volume",
                        false, x + 18, y + 58, 14, color
                    );
                    renderer->drawString(
                        "when using the built-in  Nintendo Switch speakers.",
                        false, x + 18
                        , y + 75, 14, color
                    );
                }
            );

            lockedText->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 92);
            list->addItem(lockedText);
        } else {
            mvol_slider = new ModifiedStepTrackBar("", {
                "100%", "150%", "200%", "250%", "300%"
            }, {
                &symbol::repeat::volume_100::symbol,
                &symbol::repeat::volume_150::symbol,
                &symbol::repeat::volume_200::symbol,
                &symbol::repeat::volume_250::symbol,
                &symbol::repeat::volume_300::symbol
            });

            mvol_slider->setSymbolScale(1.0f);
            mvol_slider->setSymbolOffsetX(-0);
            mvol_slider->setSymbolColor(255, 255, 255);
            mvol_slider->setStepLabelsOffsetX(10);
            mvol_slider->setStepLabelsOffsetY(10);
            mvol_slider->setStepLabelsColor(255, 255, 255);
            mvol_slider->setTrackOffsetX(10);
            mvol_slider->setTrackOffsetY(15);
            mvol_slider->setControlHeight(MasterVolumeAreaHeight);
            mvol_slider->setProgress(currentStep);

            mvol_slider->setValueChangedListener([this](std::uint8_t step) {
                mv::ipc::State state{};
                if (!mv::client::getState(state) || state.locked || !state.available) {
                    if (mvol_slider && mv::client::getState(state)) {
                        currentStep = state.step;
                        mvol_slider->setProgress(currentStep);
                    }
                    return;
                }

                if (!mv::client::setStep(step)) {
                    if (mv::client::getState(state)) {
                        currentStep = state.step;
                        if (mvol_slider)
                            mvol_slider->setProgress(currentStep);
                    }
                    return;
                }
                currentStep = step;
            });

            mvol_header->setText(
                "Master Volume:                               \ue0e2 Reset"
            );
            list->addItem(mvol_header);
            list->addItem(mvol_slider);
        }

        content_list = list;
        root_frame->setContent(content_list);
    }

    tsl::elm::OverlayFrame* root_frame = nullptr;
    tsl::elm::List* content_list = nullptr;
    tsl::elm::CategoryHeader* mvol_header = nullptr;
    ModifiedStepTrackBar* mvol_slider = nullptr;
    std::uint8_t currentStep = 0;
    u32 statePollCounter = 0;
    bool mv_ipc_connected = false;
    std::uint32_t mv_ipc_error = 0;
    bool mv_service_available = false;
    bool volume_slider_locked = true;
};

}

class MasterVolumeOverlay : public tsl::Overlay {
public:
    void initServices() override {
        mv::client::initialize();
    }

    void exitServices() override {
        mv::client::shutdown();
    }

    void onShow() override {}
    void onHide() override {}

    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<AudioControlGUI>();
    }
};

int main(int argc, char** argv) {
    return tsl::loop<MasterVolumeOverlay>(argc, argv);
}
