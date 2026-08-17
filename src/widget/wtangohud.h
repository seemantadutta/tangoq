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
    // Width the current content needs, so the widget sizes to fit and never
    // clips (used by sizeHint()).
    int contentWidth() const;

    // The flow strip's letters, one per active slot, from the hud_flow_* controls
    // (empty/unknown slots become spaces). Length is the active pattern length.
    QString flowLetters() const;

    // Must match kHudFlowMaxSlots in autodjprocessor.cpp.
    static constexpr int kMaxFlowSlots = 8;

    ControlProxy* m_pCountdownSeconds;
    ControlProxy* m_pNextKind;
    ControlProxy* m_pTandaTrackCount;
    ControlProxy* m_pTandaPlayingIndex;
    // Resolved flow strip published by AutoDJProcessor: length, highlighted slot,
    // per-slot type (0=T,1=V,2=M,3=N; -1 = empty), and the trailing "!" flag.
    ControlProxy* m_pFlowLen;
    ControlProxy* m_pFlowHighlight;
    ControlProxy* m_pFlowMismatch;
    ControlProxy* m_pFlowSlots[kMaxFlowSlots];
};
