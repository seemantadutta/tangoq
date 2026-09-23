// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

// The LIVE-mode stop guard's confirm prompt. While armed, it covers the inside
// of the host button with a short "Tap again" label, and a thin bar along its
// bottom edge shrinks from right to left over the guard window, so the DJ sees
// both what to do and how long they have. Purely visual and transparent to mouse
// events, so it can sit on top of a clickable widget (e.g. the Auto DJ button)
// without blocking it.
//
// Its colors are skin properties, so a color scheme can set them, e.g.
// WCountdownOverlay { qproperty-barColor: #d09300; }.
class WCountdownOverlay : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor backgroundColor MEMBER m_backgroundColor DESIGNABLE true)
    Q_PROPERTY(QColor textColor MEMBER m_textColor DESIGNABLE true)
    Q_PROPERTY(QColor barColor MEMBER m_barColor DESIGNABLE true)
  public:
    explicit WCountdownOverlay(QWidget* parent = nullptr);

    // Begins (or restarts) the countdown over durationMs and shows the overlay.
    // Place it over the inside of the host (within its border) first.
    void start(int durationMs);
    // Stops the animation and hides the overlay.
    void stop();

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    QElapsedTimer m_elapsed;
    QTimer m_repaintTimer;
    int m_durationMs;
    QColor m_backgroundColor;
    QColor m_textColor;
    QColor m_barColor;
};
