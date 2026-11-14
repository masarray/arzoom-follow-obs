#include "arzoom-keycap-rasterizer.hpp"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QRectF>
#include <algorithm>

namespace arzoom::presentation {
namespace {
int physical_width(KeycapSize size)
{
    switch (size) {
    case KeycapSize::Unit125: return 103;
    case KeycapSize::Unit150: return 123;
    case KeycapSize::Unit200: return 164;
    case KeycapSize::Unit350: return 287;
    case KeycapSize::Unit1:
    default: return 82;
    }
}

void paint_windows_logo(QPainter &p, qreal center_x, qreal center_y)
{
    const qreal x = center_x - 15.0;
    const qreal y = center_y - 15.0;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(38, 48, 65));
    p.drawRect(QRectF(x, y + 1, 13, 13));
    p.drawRect(QRectF(x + 16, y - 1, 13, 14));
    p.drawRect(QRectF(x, y + 16, 13, 13));
    p.drawRect(QRectF(x + 16, y + 16, 13, 14));
}
} // namespace

const QImage &KeycapRasterizer::image(const KeyboardOverlayFrame &frame)
{
    if (!image_.isNull() && frame.generation == generation_)
        return image_;

    if (image_.isNull())
        image_ = QImage(kWidth, kHeight, QImage::Format_RGBA8888);
    image_.fill(Qt::transparent);
    if (frame.keycap_count == 0) {
        generation_ = frame.generation;
        return image_;
    }

    constexpr int kSpacing = 12;
    constexpr int kKeyHeight = 82;
    constexpr int kTop = 17;
    int total_width = 0;
    for (std::size_t i = 0; i < frame.keycap_count; ++i)
        total_width += physical_width(frame.keycaps[i].size);
    total_width += kSpacing * static_cast<int>(frame.keycap_count - 1);

    QPainter p(&image_);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    QFont font(QStringLiteral("Segoe UI"), 21);
    font.setWeight(QFont::DemiBold);
    p.setFont(font);

    int x = (kWidth - total_width) / 2;
    for (std::size_t i = 0; i < frame.keycap_count; ++i) {
        const auto &key = frame.keycaps[i];
        const int width = physical_width(key.size);
        const QRectF body(x, kTop, width, kKeyHeight);

        /* Physically lifted light key, 6px depth, no bulky overlay card. */
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(10, 21, 35, 65));
        p.drawRoundedRect(body.translated(0, 9), 14, 14);
        p.setBrush(QColor(95, 110, 133, 215));
        p.drawRoundedRect(body.translated(0, 6), 13, 13);

        QLinearGradient face(0, kTop, 0, kTop + kKeyHeight);
        face.setColorAt(0.0, QColor(255, 255, 255));
        face.setColorAt(0.52, QColor(250, 252, 255));
        face.setColorAt(1.0, QColor(220, 227, 236));
        p.setBrush(QBrush(face));
        p.setPen(QPen(QColor(158, 170, 186, 220), 1.5));
        p.drawRoundedRect(body, 13, 13);

        p.setPen(QPen(QColor(255, 255, 255, 210), 1.3));
        p.drawLine(QPointF(x + 16, kTop + 6),
                   QPointF(x + width - 16, kTop + 6));

        if (key.glyph == KeycapGlyph::WindowsLogo) {
            paint_windows_logo(p, x + width / 2.0,
                               kTop + kKeyHeight / 2.0);
        } else {
            const QString label = QString::fromWCharArray(key.label.data());
            QFont fitted = font;
            if (label.size() > 5)
                fitted.setPointSize(16);
            else if (label.size() > 4)
                fitted.setPointSize(18);
            p.setFont(fitted);
            p.setPen(QColor(39, 49, 66));
            p.drawText(body, Qt::AlignCenter, label);
            p.setFont(font);
        }
        x += width + kSpacing;
    }
    p.end();
    generation_ = frame.generation;
    return image_;
}

} // namespace arzoom::presentation
