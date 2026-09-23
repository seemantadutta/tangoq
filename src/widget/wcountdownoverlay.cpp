// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "widget/wcountdownoverlay.h"

#include <QFontMetrics>
#include <QPainter>

#include "moc_wcountdownoverlay.cpp"

namespace {
constexpr int kFrameIntervalMs = 33; // ~30 fps
// Height of the countdown bar along the bottom edge.
constexpr int kBarHeight = 3;
// The label shrinks to fit small buttons, within these sizes.
constexpr int kMaxLabelPixelSize = 12;
constexpr int kMinLabelPixelSize = 8;
// Defaults suit the dark Default scheme; the bar matches its active orange.
const QColor kDefaultBackground(0x1b, 0x1b, 0x1d);
const QColor kDefaultText(0xff, 0xff, 0xff);
const QColor kDefaultBar(0xb2, 0x4c, 0x12);
} // anonymous namespace

WCountdownOverlay::WCountdownOverlay(QWidget* parent)
        : QWidget(parent),
          m_durationMs(0),
          m_backgroundColor(kDefaultBackground),
          m_textColor(kDefaultText),
          m_barColor(kDefaultBar) {
    // Sit on top of the host widget without intercepting its clicks.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
    m_repaintTimer.setInterval(kFrameIntervalMs);
    connect(&m_repaintTimer, &QTimer::timeout, this, [this]() {
        if (m_elapsed.isValid() && m_elapsed.elapsed() >= m_durationMs) {
            // Hold the empty state; the owner hides us when the guard disarms.
            m_repaintTimer.stop();
        }
        update();
    });
}

void WCountdownOverlay::start(int durationMs) {
    m_durationMs = durationMs > 0 ? durationMs : 0;
    m_elapsed.start();
    m_repaintTimer.start();
    show();
    raise();
    update();
}

void WCountdownOverlay::stop() {
    m_repaintTimer.stop();
    hide();
}

void WCountdownOverlay::paintEvent(QPaintEvent* /*event*/) {
    double remaining = 1.0;
    if (m_durationMs > 0 && m_elapsed.isValid()) {
        remaining = 1.0 - static_cast<double>(m_elapsed.elapsed()) / m_durationMs;
    }
    remaining = qBound(0.0, remaining, 1.0);

    QPainter painter(this);
    painter.fillRect(rect(), m_backgroundColor);

    // The label, centered above the bar, at the largest size that fits.
    const QString label = tr("Tap again");
    const QRect labelRect(0, 0, width(), height() - kBarHeight);
    QFont font = painter.font();
    font.setBold(true);
    for (int size = kMaxLabelPixelSize; size >= kMinLabelPixelSize; --size) {
        font.setPixelSize(size);
        if (QFontMetrics(font).horizontalAdvance(label) <= labelRect.width() - 4) {
            break;
        }
    }
    painter.setFont(font);
    painter.setPen(m_textColor);
    painter.drawText(labelRect, Qt::AlignCenter, label);

    // The bar shrinks from right to left as the time runs out.
    const int barWidth = static_cast<int>(width() * remaining + 0.5);
    if (barWidth > 0) {
        painter.fillRect(QRect(0, height() - kBarHeight, barWidth, kBarHeight),
                m_barColor);
    }
}
