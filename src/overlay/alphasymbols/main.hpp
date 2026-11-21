// Este script me permite dibujar simbolos imagenes lo que sea, en la GUI del overlay
// pero me limita mucho :v ya que solo puedo dibujar simbolos alpha, me gustaria renderizar
// imagenes .png o no se si eso sea posible aun no e investigado, eso, lo importante es que 
// mientras tanto la interfaz no queda vacia y puedo dibujar cosas, aunque sean simples,
// pero es mejor que nada :v

#pragma once
 
#include <algorithm>
#include <cmath>
#include <cstddef>
 
#include <tesla.hpp>

class AlphaSymbol {
  public:
    constexpr AlphaSymbol(size_t width, size_t height, const u8 *data)
        : m_width(static_cast<s32>(width)),
          m_height(static_cast<s32>(height)),
          m_data(data) {}
 

    void draw(s32 centerX, s32 centerY, tsl::gfx::Renderer *renderer, tsl::Color color, float scale = 1.0f) const {
        const s32 scaledWidth  = scaledSize(m_width, scale);
        const s32 scaledHeight = scaledSize(m_height, scale);
 

        const s32 left = centerX - scaledWidth / 2;
        const s32 top  = centerY - scaledHeight / 2;
 
        for (s32 row = 0; row < scaledHeight; row++) {
            for (s32 col = 0; col < scaledWidth; col++) {
                const u16 alpha = sourceAlpha(col, row, scaledWidth, scaledHeight);
                if (alpha == 0)
                    continue;

                color.a = alpha;
                const tsl::Color pixelColor = ult::disableTransparency
                    ? color
                    : renderer->a(color);
                renderer->setPixelBlendSrc(left + col, top + row, pixelColor);
            }
        }
    }
 
  private:
    static constexpr u8 ALPHA_SHIFT = 4;
 
    static s32 scaledSize(s32 size, float scale) {
        return std::max<s32>(1, static_cast<s32>(std::round(size * scale)));
    }
 
    u16 sourceAlpha(s32 col, s32 row, s32 scaledWidth, s32 scaledHeight) const {
        const s32 sourceX = col * m_width / scaledWidth;
        const s32 sourceY = row * m_height / scaledHeight;
        const u8 alpha8bit = m_data[sourceY * m_width + sourceX];
        return static_cast<u16>(alpha8bit >> ALPHA_SHIFT);
    }
 
    s32 m_width;
    s32 m_height;
    const u8 *m_data;
};
