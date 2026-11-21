// La barra y mi dolor de cabeza JAJAJA, mientras creaba esta barra por pasos, tuve que ajustar muchos valores
// para que pareciera como si nintendo la hubiera hecho, y no como si la hubiera hecho un programador novato :v

// Mi perfeccionismo no me dejo poner una simple barra, asi que me tome mi tiempo para pulirla y que se vea lo
// mas parecido a la barra de volumen del sistema, aunque no es exacta, pero si muy parecida!

#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>

#include <switch.h>
#include <tesla.hpp>
#include "../alphasymbols/main.hpp"

class ModifiedStepTrackBar : public tsl::elm::StepTrackBar {
public:
    ModifiedStepTrackBar(
        const char *icon,
        std::initializer_list<std::string> stepDescriptions,
        std::initializer_list<const AlphaSymbol *> stepSymbols
    )
        : StepTrackBar(icon, stepDescriptions.size()),
          savedStepDescriptions(stepDescriptions.begin(), stepDescriptions.end()),
          cachedWidths(stepDescriptions.size(), -1),
          savedStepSymbols(stepSymbols.begin(), stepSymbols.end()) { }

    void setTrackOffsetY(s32 offset) {
        trackOffsetY = offset;
        invalidate();
    }

    void setTrackOffsetX(s32 offset) {
        trackOffsetX = offset;
        invalidate();
    }

    void setControlHeight(u16 height) {
        controlHeight = height;
        invalidate();
    }

    void layout(u16, u16, u16, u16) override {
        setBoundaries(getX(), getY(), getWidth(), controlHeight);
    }

    void setSymbolScale(float scale) {
        symbolScale = std::clamp(scale, 0.25f, 4.0f);
        invalidate();
    }

    void setSymbolOffsetX(s32 offset) {
        symbolOffsetX = offset;
        invalidate();
    }

    void setSymbolOffsetY(s32 offset) {
        symbolOffsetY = offset;
        invalidate();
    }

    void setStepLabelsOffsetY(s32 offset) {
        stepLabelsOffsetY = offset;
        invalidate();
    }

    void setStepLabelsOffsetX(s32 offset) {
        stepLabelsOffsetX = offset;
        invalidate();
    }

    void setSymbolColor(u8 red, u8 green, u8 blue) {
        symbolColor = rgbColor(red, green, blue);
        invalidate();
    }

    void setStepLabelsColor(u8 red, u8 green, u8 blue) {
        stepLabelsColor = rgbColor(red, green, blue);
        invalidate();
    }

    void draw(tsl::gfx::Renderer *renderer) override {
        const s32 trackY = getY() + 42 + trackOffsetY;
        const s32 trackStart = getX() + 60 + trackOffsetX;
        const s32 trackWidth = getWidth() - 95;
        const s32 trackEnd = trackStart + trackWidth;
        const s32 labelTrackStart = getX() + 60;

        const auto separatorColor = renderer->a(tsl::separatorColor);
        renderer->drawRect(getX() + 7, getTopBound(), getWidth() + 1, 1, separatorColor);
        renderer->drawRect(getX() + 7, getBottomBound(), getWidth() + 1, 1, separatorColor);
        renderer->drawString(m_icon, false, getX() + 15, getY() + 50, 23, renderer->a(tsl::style::color::ColorText));

        const s32 handlePos = trackWidth * static_cast<float>(m_value) / 100;
        renderer->drawCircle(trackStart, trackY, 2, true, renderer->a(tsl::style::color::ColorHighlight));
        renderer->drawCircle(trackEnd, trackY, 2, true, renderer->a(tsl::style::color::ColorFrame));
        renderer->drawRect(trackStart + handlePos, trackY - 2, trackWidth - handlePos, 5, renderer->a(tsl::style::color::ColorFrame));
        renderer->drawRect(trackStart, trackY - 2, handlePos, 5, renderer->a(tsl::style::color::ColorHighlight));
        renderer->drawCircle(trackStart + 2 + handlePos, trackY, 18, true, renderer->a(tsl::style::color::ColorHandle));
        renderer->drawCircle(trackStart + 2 + handlePos, trackY, 18, false, renderer->a(tsl::style::color::ColorFrame));

        const auto numSteps = savedStepDescriptions.size();
        if (numSteps == 0)
            return;

        for (size_t i = 0; i < numSteps; ++i) {
            const float fraction = (numSteps == 1)
                ? 0.0f
                : static_cast<float>(i) / static_cast<float>(numSteps - 1);
            const s32 stepCenterX = labelTrackStart + 2 + static_cast<s32>(trackWidth * fraction);
            const char *text = savedStepDescriptions[i].c_str();

            if (cachedWidths[i] < 0) {
                auto [descWidth, descHeight] = renderer->drawString(
                    text, false, 0, 0, 15,
                    tsl::style::color::ColorTransparent
                );
                cachedWidths[i] = descWidth;
            }

            renderer->drawString(
                text, false,
                stepCenterX - (cachedWidths[i] / 2) + stepLabelsOffsetX,
                getY() + 20 + stepLabelsOffsetY, 15,
                renderer->a(stepLabelsColor)
            );

        }

        const auto selectedStep = getProgress();
        if (selectedStep < savedStepSymbols.size() && savedStepSymbols[selectedStep] != nullptr) {
            savedStepSymbols[selectedStep]->draw(
                getX() + 30 + symbolOffsetX,
                getY() + (getHeight() / 2) + symbolOffsetY, renderer,
                symbolColor, symbolScale
            );
        }
    }

    bool onTouch(
        tsl::elm::TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY,
        s32 initialX, s32 initialY
    ) override {
        if (event == tsl::elm::TouchEvent::Release)
            return false;
        if (savedStepDescriptions.empty() || !inBounds(initialX, initialY) ||
            currY < getTopBound() || currY > getBottomBound())
            return false;

        const s32 trackStart = getX() + 60 + trackOffsetX;
        const s32 trackWidth = getWidth() - 95;
        if (trackWidth <= 0)
            return false;

        const s32 clampedX = std::clamp(currX, trackStart, trackStart + trackWidth);
        const float fraction = static_cast<float>(clampedX - trackStart) / trackWidth;
        const auto step = static_cast<u8>(std::round(fraction * (savedStepDescriptions.size() - 1)));
        if (step != getProgress()) {
            setProgress(step);
            m_valueChangedListener(getProgress());
        }
        return true;
    }

    void drawHighlight(tsl::gfx::Renderer *renderer) override {
        static float counter = 0;
        const float progress = (std::sin(counter) + 1) / 2;
        tsl::Color highlightColor = {
            static_cast<u8>((0x2 - 0x8) * progress + 0x8),
            static_cast<u8>((0x8 - 0xF) * progress + 0xF),
            static_cast<u8>((0xC - 0xF) * progress + 0xF),
            static_cast<u8>((0x6 - 0xD) * progress + 0xD)
        };
        counter += 0.1F;

        const s32 trackY = getY() + 42 + trackOffsetY;
        const s32 handlePos = (getWidth() - 95) * static_cast<float>(m_value) / 100;
        for (u8 radius = 16; radius <= 19; ++radius)
            renderer->drawCircle(getX() + 62 + trackOffsetX + handlePos, trackY, radius, false, renderer->a(highlightColor));
    }

private:
    static tsl::Color rgbColor(u8 red, u8 green, u8 blue) {
        
        return {static_cast<u8>(red >> 4), static_cast<u8>(green >> 4),
                static_cast<u8>(blue >> 4), 0xF};
    }

    std::vector<std::string> savedStepDescriptions;
    std::vector<s32> cachedWidths;
    std::vector<const AlphaSymbol *> savedStepSymbols;
    u16 controlHeight = tsl::style::TrackBarDefaultHeight;
    s32 trackOffsetY = 0;
    s32 trackOffsetX = 0;
    float symbolScale = 1.0f;
    s32 symbolOffsetX = 0;
    s32 symbolOffsetY = 0;
    s32 stepLabelsOffsetY = 0;
    s32 stepLabelsOffsetX = 0;
    tsl::Color symbolColor = tsl::style::color::ColorDescription;
    tsl::Color stepLabelsColor = tsl::style::color::ColorDescription;
};
