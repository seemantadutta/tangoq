#pragma once

#include "widget/wwidget.h"

class QDomNode;
class SkinContext;
class ControlProxy;

// Tango HUD: a toolbar heads-up display for the milonga DJ. Paints (so it stays
// crisp at any DPI, unlike a QSS image) a two-line countdown - a label over a
// large time so it is hard to miss - with the current tanda's track pips beside
// it. Reads live state from the [AutoDJ],hud_* controls published by
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
    // Width the current content needs, so the widget sizes to fit and never
    // clips (used by sizeHint()).
    int contentWidth() const;

    ControlProxy* m_pCountdownSeconds;
    ControlProxy* m_pNextKind;
    ControlProxy* m_pTandaTrackCount;
    ControlProxy* m_pTandaPlayingIndex;
};
