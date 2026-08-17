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
