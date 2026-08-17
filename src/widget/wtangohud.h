#pragma once

#include "widget/wwidget.h"

class QDomNode;
class SkinContext;

// Tango HUD: a toolbar heads-up display for the milonga DJ. Paints (so it stays
// crisp at any DPI, unlike a QSS image) a countdown line, the T/V/M flow with
// the current tanda in red, and the track-in-tanda pips (matching the Auto DJ
// list). This first cut paints static placeholder values; live data from
// AutoDJProcessor is wired next.
class WTangoHud : public WWidget {
    Q_OBJECT
  public:
    explicit WTangoHud(QWidget* pParent = nullptr);

    void setup(const QDomNode& node, const SkinContext& context);

    QSize sizeHint() const override;

  protected:
    void paintEvent(QPaintEvent* pEvent) override;
};
