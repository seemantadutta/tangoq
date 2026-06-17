#include "widget/wcountdownoverlay.h"

#include <QColor>
#include <QPainter>

#include "moc_wcountdownoverlay.cpp"

namespace {
constexpr int kFrameIntervalMs = 33; // ~30 fps
} // anonymous namespace

WCountdownOverlay::WCountdownOverlay(QWidget* parent)
        : QWidget(parent),
          m_durationMs(0) {
    // Sit on top of the host widget without intercepting its clicks. The widget is
    // opaque and paints a snapshot of the host behind the liquid (see start()), so
    // the drained part matches the host exactly.
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

void WCountdownOverlay::start(int durationMs, const QPixmap& background) {
    m_durationMs = durationMs > 0 ? durationMs : 0;
    m_background = background;
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

    // Snapshot of the host behind the overlay: fills the whole widget so the
    // drained part is pixel-identical to the host (the Auto DJ button).
    if (!m_background.isNull()) {
        painter.drawPixmap(0, 0, m_background);
    }

    // Red "liquid" filling the bottom of the button; its surface (top edge) falls
    // from full to empty as the time runs out, like a leaking container. The side
    // and bottom edges are drawn past the widget bounds so they stay crisp; only
    // the horizontal surface moves (antialiased for smooth sub-pixel motion).
    const double level = height() * remaining;
    if (level > 0.0) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        // Translucent so the icon (snapshot) shows through the red tint.
        painter.setBrush(QColor(0xee, 0x22, 0x22, 190));
        painter.drawRect(QRectF(-1.0, height() - level, width() + 2.0, level + 1.0));
    }
}
