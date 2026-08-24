// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QChar>
#include <QColor>
#include <QPainter>
#include <QPen>
#include <QRectF>

namespace mixxx {

// Geometry of the tanda progress pips, shared by the Auto DJ list header
// (WTandaQueueView) and the toolbar HUD (WTangoHud) so both look identical.
constexpr int kTandaProgressPipDiameter = 8;
constexpr int kTandaProgressPipGap = 5;

// Draws one tanda progress pip. State: '1' played (filled disc), 'h' currently
// playing (left-half pie), anything else unplayed (outline only).
inline void drawTandaProgressPip(
        QPainter* pPainter, const QRectF& rect, const QColor& color, QChar state) {
    QPen pen(color);
    pen.setWidth(1);
    pPainter->setPen(pen);
    pPainter->setBrush(Qt::NoBrush);
    pPainter->drawEllipse(rect);

    if (state == QLatin1Char('1')) {
        pPainter->setBrush(color);
        pPainter->drawEllipse(rect);
    } else if (state == QLatin1Char('h')) {
        pPainter->setBrush(color);
        pPainter->drawPie(rect, 90 * 16, 180 * 16);
    }

    pPainter->setBrush(Qt::NoBrush);
    pPainter->drawEllipse(rect);
}

} // namespace mixxx
