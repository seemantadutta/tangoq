#pragma once

#include <QTimer>

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

    // True when the countdown is in its final-30 s flash window (0 <= s < 30).
    bool inFlashWindow() const;

    // Drives the final-30 s red "breathe" of the time value.
    QTimer m_flashTimer;
    // Breath phase in [0, 1); advanced by the timer while inside the window.
    double m_breathPhase{0.0};

    ControlProxy* m_pCountdownSeconds;
    ControlProxy* m_pNextKind;
    ControlProxy* m_pTandaTrackCount;
    ControlProxy* m_pTandaPlayingIndex;
    // Auto DJ running state. The HUD only has something to say while a set is
    // playing, so when this is off it keeps its reserved size but paints nothing.
    ControlProxy* m_pAutoDJEnabled;
    // Settings-panel toggles (default on): hide the countdown timer (label +
    // time) and/or the tanda progress pips independently. The widget keeps its
    // reserved size either way so the toolbar never reflows.
    ControlProxy* m_pShowCountdownTimer;
    ControlProxy* m_pShowProgressPips;
};
