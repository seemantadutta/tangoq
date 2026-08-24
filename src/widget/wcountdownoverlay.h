// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QElapsedTimer>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

// A countdown overlay that drains like liquid: it starts filled solid red and the
// red "level" falls to empty over a fixed duration, revealing the host beneath.
// Purely visual and transparent to mouse events, so it can sit on top of a
// clickable widget (e.g. the Auto DJ button) without blocking it.
//
// It paints a caller-supplied snapshot of the area behind it as its background, so
// the drained part is pixel-identical to the host (no transparency artifacts), and
// the only moving edge is a straight horizontal liquid surface (no jagged edges).
class WCountdownOverlay : public QWidget {
    Q_OBJECT
  public:
    explicit WCountdownOverlay(QWidget* parent = nullptr);

    // Begins (or restarts) the countdown over durationMs and shows the overlay.
    // background must be a snapshot of the host area covered by this widget.
    void start(int durationMs, const QPixmap& background);
    // Stops the animation and hides the overlay.
    void stop();

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    QElapsedTimer m_elapsed;
    QTimer m_repaintTimer;
    int m_durationMs;
    QPixmap m_background;
};
