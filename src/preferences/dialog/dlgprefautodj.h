#pragma once

#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/dialog/ui_dlgprefautodjdlg.h"
#include "preferences/usersettings.h"

class QWidget;
class ControlProxy;
class AutoDJFeature;

class DlgPrefAutoDJ : public DlgPreferencePage, public Ui::DlgPrefAutoDJDlg {
    Q_OBJECT
  public:
    DlgPrefAutoDJ(QWidget* pParent,
            UserSettingsPointer pConfig,
            AutoDJFeature* pAutoDJFeature);

    bool okayToClose() const override;

  public slots:
    void slotUpdate() override;
    void slotApply() override;
    void slotResetToDefaults() override;
    void slotCancel() override;

  private slots:
    void slotSetCortinaLength(int);
    void slotSetCortinaFadeMode(int);
    void slotSetCortinaFadeIn(int);
    void slotSetCortinaFadeOut(int);
    void slotSetMinimumAvailable(int);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    void slotToggleRequeueIgnore(Qt::CheckState state);
#else
    void slotToggleRequeueIgnore(int buttonState);
#endif
    void slotSetRequeueIgnoreTime(const QTime& a_rTime);
    void slotSetRandomQueueMin(int);
    void slotStateMonitorToggled(bool enabled);
    void slotConsiderRepeatPlaylistState(bool);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    void slotToggleRandomQueue(Qt::CheckState state);
#else
    void slotToggleRandomQueue(int buttonState);
#endif

  private:
    // Refreshes the read-only "Cortina hold time" (Y = cortina length - fade-in
    // - fade-out) label from the current buffered fade-in/out and cortina length.
    void updateCortinaHoldLabel();
    // Enables/disables the cortina fade-in/out inputs depending on whether
    // the Cortina Fade transition mode is selected.
    void updateCortinaFadeEnabled();
    void updateStateMonitorStatus();

    UserSettingsPointer m_pConfig;
    AutoDJFeature* const m_pAutoDJFeature;
    bool m_stateMonitorConfigValid{true};
    // Observes the live [AutoDJ],cortina_length so the (stop-only) length field
    // reflects cockpit nudges made while Auto DJ is running, even though it stays
    // greyed out then.
    ControlProxy* m_pCortinaLengthControl;
};
