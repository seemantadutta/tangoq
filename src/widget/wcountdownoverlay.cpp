// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "widget/wcountdownoverlay.h"

#include <QPainter>

#include "moc_wcountdownoverlay.cpp"

namespace {
constexpr int kFrameIntervalMs = 33; // ~30 fps
} // anonymous namespace

WCountdownOverlay::WCountdownOverlay(QWidget* parent)
        : QWidget(parent),
          m_durationMs(0) {
    // Sit on top of the host widget without intercepting its clicks. The widget is
    // opaque and paints snapshots of the host (see start()), so both the full
    // and the drained part match the host exactly.
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

void WCountdownOverlay::start(int durationMs,
        const QPixmap& fullSnapshot,
        const QPixmap& drainedSnapshot) {
    m_durationMs = durationMs > 0 ? durationMs : 0;
    m_fullSnapshot = fullSnapshot;
    m_drainedSnapshot = drainedSnapshot;
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

    // The drained part is the host in its "off" state, filling the whole widget.
    if (!m_drainedSnapshot.isNull()) {
        painter.drawPixmap(0, 0, m_drainedSnapshot);
    }

    // The liquid is the host in its "on" state, filling the bottom; its surface
    // (top edge) falls from full to empty as the time runs out, like a leaking
    // container. Only that horizontal surface moves.
    const int level = static_cast<int>(height() * remaining + 0.5);
    if (level > 0 && !m_fullSnapshot.isNull()) {
        const qreal ratio = m_fullSnapshot.devicePixelRatio();
        const int top = height() - level;
        painter.drawPixmap(QRectF(0, top, width(), level),
                m_fullSnapshot,
                QRectF(0, top * ratio, width() * ratio, level * ratio));
    }
}
