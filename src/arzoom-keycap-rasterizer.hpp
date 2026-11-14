#pragma once
#include "arzoom-shortcut.hpp"

#include <QImage>
#include <cstdint>

namespace arzoom::presentation {

/* One retained 960x144 RGBA image. Rasterization is event-driven, never
 * in the per-pixel shader and never repeated during hold/fade frames. */
class KeycapRasterizer {
public:
    static constexpr int kWidth = 960;
    static constexpr int kHeight = 144;

    const QImage &image(const KeyboardOverlayFrame &frame);
    void invalidate() noexcept { generation_ = 0; }

private:
    QImage image_{};
    std::uint32_t generation_ = 0;
};

} // namespace arzoom::presentation
