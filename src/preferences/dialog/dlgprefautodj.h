#pragma once

#include <QColor>
#include <array>

#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/dialog/ui_dlgprefautodjdlg.h"
#include "preferences/usersettings.h"

class QWidget;
class QCheckBox;
class ControlProxy;
class QPushButton;
class TandaColorPalette;

class DlgPrefAutoDJ : public DlgPreferencePage, public Ui::DlgPrefAutoDJDlg {
    Q_OBJECT
  public:
    DlgPrefAutoDJ(QWidget* pParent, UserSettingsPointer pConfig);

    // Blocks OK/Apply while the cortina fade budget is invalid (fade-in +
    // fade-out exceed the cortina length in Cortina Fade mode). DlgPreferences
    // then keeps the dialog open and switches back to this page.
    bool okayToClose() const override;

  public slots:
    void slotUpdate() override;
    void slotApply() override;
    void slotResetToDefaults() override;
    void slotCancel() override;

  private slots:
    void slotSetCortinaLength(int);
    void slotSetCortinaLevel(int);
    void slotSetCortinaFadeMode(int);
    void slotSetCortinaFadeIn(int);
    void slotSetCortinaFadeOut(int);
    void slotSetShowFadeEnvelope(bool);
    void slotSetMinimumAvailable(int);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    void slotToggleRequeueIgnore(Qt::CheckState state);
#else
    void slotToggleRequeueIgnore(int buttonState);
#endif
    void slotSetRequeueIgnoreTime(const QTime& a_rTime);
    void slotSetRandomQueueMin(int);
    void slotConsiderRepeatPlaylistState(bool);
    void resetTandaColorsToDefaults();
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
    // Locks the cortina length, transition mode and fade inputs while Auto DJ
    // is running, and unlocks them when it stops. Kept in its own method so it
    // can be re-run live when [AutoDJ],enabled changes, not only on dialog show.
    void updateCortinaControlsEnabled();
    // True unless Cortina Fade mode is selected and fade-in + fade-out exceed
    // the cortina length. Reads the currently shown (buffered) spinbox values.
    bool cortinaFadeBudgetValid() const;
    void setupTandaColorEditors();
    void loadTandaColors();
    void updateTandaColorEditorsEnabled();
    void updateTandaColorEditor(int index);
    void chooseTandaColor(int index);

    static constexpr int kTandaColorCount = 6;

    UserSettingsPointer m_pConfig;
    // Observes the live [AutoDJ],cortina_length so the (stop-only) length field
    // reflects cockpit nudges made while Auto DJ is running, even though it stays
    // greyed out then.
    ControlProxy* m_pCortinaLengthControl;
    // Observes the live [AutoDJ],cortina_level_db so the level field reflects
    // changes made with the toolbar Level buttons.
    ControlProxy* m_pCortinaLevelControl;
    // Observes [AutoDJ],enabled so the cortina timing controls lock/unlock live
    // when a set starts or stops while the preferences dialog is already open.
    ControlProxy* m_pAutoDJEnabledControl;
    TandaColorPalette* const m_pTandaColorPalette;
    bool m_tandaColorCodingEnabled{true};
    QCheckBox* m_pUseTandaColorCodingCheckBox{nullptr};
    QWidget* m_pTandaColorEditors{nullptr};
    std::array<QColor, kTandaColorCount> m_tandaColors;
    std::array<QPushButton*, kTandaColorCount> m_pTandaColorButtons{};
};
