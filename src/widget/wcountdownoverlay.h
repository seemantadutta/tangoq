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

// A countdown overlay that drains like liquid: the host's "on" look (e.g. the
// orange of a running Auto DJ or playing deck button) empties from the top over a
// fixed duration, revealing the host's "off" look. Purely visual and transparent
// to mouse events, so it can sit on top of a clickable widget (e.g. the Auto DJ
// button) without blocking it.
//
// It paints two caller-supplied snapshots of the host, so both parts are
// pixel-identical to the real button in each state, in every color scheme, and
// the only moving edge is a straight horizontal liquid surface.
class WCountdownOverlay : public QWidget {
    Q_OBJECT
  public:
    explicit WCountdownOverlay(QWidget* parent = nullptr);

    // Begins (or restarts) the countdown over durationMs and shows the overlay.
    // fullSnapshot and drainedSnapshot are the host area covered by this widget,
    // rendered in its current ("on") and its "off" state respectively.
    void start(int durationMs,
            const QPixmap& fullSnapshot,
            const QPixmap& drainedSnapshot);
    // Stops the animation and hides the overlay.
    void stop();

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    QElapsedTimer m_elapsed;
    QTimer m_repaintTimer;
    int m_durationMs;
    QPixmap m_fullSnapshot;
    QPixmap m_drainedSnapshot;
};
