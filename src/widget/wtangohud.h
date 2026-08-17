#pragma once

#include "widget/wwidget.h"

class QDomNode;
class SkinContext;
class ControlProxy;

// Tango HUD: a toolbar heads-up display for the milonga DJ. Paints (so it stays
// crisp at any DPI, unlike a QSS image) a countdown line, the T/V/M flow with
// the current tanda in red, and the track-in-tanda pips (matching the Auto DJ
// list). It reads live state from the [AutoDJ],hud_* controls published by
// AutoDJProcessor and TandaQueueModel.
class WTangoHud : public WWidget {
    Q_OBJECT
  public:
    explicit WTangoHud(QWidget* pParent = nullptr);

    void setup(const QDomNode& node, const SkinContext& context);

    QSize sizeHint() const override;

  protected:
    void paintEvent(QPaintEvent* pEvent) override;

  private slots:
    void slotControlChanged(double value);

  private:
    ControlProxy* m_pCountdownSeconds;
    ControlProxy* m_pNextIsCortina;
    ControlProxy* m_pTandaTrackCount;
    ControlProxy* m_pTandaPlayingIndex;
    ControlProxy* m_pFlowIndex;
};
