#pragma once

#include <QDateTime>
#include <QString>
#include <QWidget>

#include "control/controlproxy.h"
#include "library/autodj/autodjprocessor.h"
#include "library/autodj/ui_dlgautodj.h"
#include "library/libraryview.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"

class PlaylistTableModel;
class AutoDJFeature;
class TandaQueueModel;
class WLibrary;
class WTrackTableView;
class Library;
class KeyboardEventFilter;
class WCountdownOverlay;
class QTimer;

class DlgAutoDJ : public QWidget, public Ui::DlgAutoDJ, public LibraryView {
    Q_OBJECT
  public:
    DlgAutoDJ(WLibrary* parent,
            UserSettingsPointer pConfig,
            Library* pLibrary,
            AutoDJFeature* pAutoDJFeature,
            AutoDJProcessor* pProcessor,
            KeyboardEventFilter* pKeyboard);
    ~DlgAutoDJ() override;

    void onShow() override;
    bool hasFocus() const override;
    void setFocus() override;
    void pasteFromSidebar() override;
    void onSearch(const QString& text) override;
    void saveCurrentViewState() override;
    bool restoreCurrentViewState() override;

  public slots:
    void shufflePlaylistButton(bool buttonChecked);
    void skipNextButton(bool buttonChecked);
    void fadeNowButton(bool buttonChecked);
    void toggleAutoDJButton(bool enable);
    void autoDJError(AutoDJProcessor::AutoDJError error);
    void transitionTimeChanged(int time);
    void transitionSliderChanged(int value);
    void tandaGapChanged(int value);
    void autoDJStateChanged(AutoDJProcessor::AutoDJState state);
    void updateSelectionInfo();
    void slotTransitionModeChanged(int comboboxIndex);
    void slotRepeatPlaylistChanged(bool checked);
    void slotEndTimeChanged(const QTime& time);

  signals:
    void addRandomTrackButton(bool buttonChecked);
    void loadTrack(TrackPointer tio);
    void loadTrackToPlayer(TrackPointer tio, const QString& group, bool);
    void trackSelected(TrackPointer pTrack);

  private:
    void setupActionButton(QPushButton* pButton,
            void (DlgAutoDJ::*pSlot)(bool),
            const QString& fallbackText);
    // Refreshes the Tango DJ mode indicator and disables/greys the controls
    // that don't make sense in Tango mode (Shuffle, Add Random, Repeat, Skip,
    // column sorting), reading the mode from the config.
    void refreshTangoModeUi();
    // Mirrors the processor's transition mode into the combo box without
    // overwriting a DJ's manual stock-mode override while Tango remains on.
    void syncTransitionModeFromProcessor();
    // Adds/removes the Tango-only Tanda Transition option from the mode combo.
    void refreshTransitionModeOptions();
    // Shows the stock transition time control or the Tanda gap control based on
    // the selected transition mode.
    void refreshTransitionControls();
    // Marks the currently playing track (red) in the Auto DJ list in Tango mode.
    void updateNowPlaying();
    // Refreshes the Tango DJ mode set end-time / time-left readout, including the
    // over/under delta against the target end time.
    void updateSetEndTime();
    // Builds the colored over/under delta text comparing the projected end against
    // the target end time (endTimeEdit), e.g. "▲ +0:04:20 over".
    QString formatEndTimeDelta(const QDateTime& projectedEnd) const;
    // Refreshes the LIVE indicator (red when on, greyed when off) and applies the
    // matching deck play/pause (D/L) keyboard suppression.
    void refreshLiveMode();
    // Right-click menu on the LIVE indicator to deliberately enter/exit LIVE mode.
    void showLiveContextMenu(const QPoint& pos);
    // Shows/clears the "Confirm Stop?" prompt on the Auto DJ button when the
    // LIVE-mode stop guard arms/disarms.
    void slotStopGuardArmedChanged(bool armed);
    void keyPressEvent(QKeyEvent* pEvent) override;

    const UserSettingsPointer m_pConfig;

    AutoDJProcessor* const m_pAutoDJProcessor;
    WTrackTableView* const m_pTrackTableView;
    const bool m_bShowButtonText;

    PlaylistTableModel* m_pAutoDJTableModel;
    TandaQueueModel* m_pTandaQueueModel;

    // Observes [AutoDJ],keep_queue (Tango DJ mode) so the toolbar refreshes
    // immediately when it is toggled in Preferences.
    ControlProxy* m_pKeepQueueControl;

    // Live cortina-length budget, shared with Preferences. The cockpit − / +
    // buttons nudge it by ±2 s (the only way to change it mid-set); the read-only
    // value label mirrors it. Tango-only.
    ControlProxy* m_pCortinaLengthControl;
    // Bumps the cortina length by delta seconds, clamped to [5, 600].
    void nudgeCortinaLength(int delta);
    // Refreshes the cockpit cortina-length value label from the control.
    void updateCortinaLengthReadout();

    // The app keyboard filter, used to suppress the deck play/pause keys (D/L)
    // while LIVE mode is on. Not owned.
    KeyboardEventFilter* const m_pKeyboard;
    // Observes [AutoDJ],live_mode so the LIVE indicator and key suppression track
    // the session-only LIVE state.
    ControlProxy* m_pLiveModeControl;
    // Auto DJ cockpit control-visibility toggles ([TangoQ],show_adj_*), set from
    // the skin Settings panel and owned by AutoDJProcessor. refreshTangoModeUi
    // hides the set-time readout, the end-time block, or the cortina nudge
    // controls when the matching toggle is off.
    ControlProxy* m_pShowAdjSetTime;
    ControlProxy* m_pShowAdjEndTime;
    ControlProxy* m_pShowAdjNudge;
    // Liquid-drain countdown overlay on the Auto DJ button while the LIVE-mode
    // stop guard is armed (parented to the button). Owned by the button.
    WCountdownOverlay* m_pStopCountdown;

    // Ticks once a second to keep the Tango set end-time readout current. Only
    // runs while Tango mode is on (see refreshTangoModeUi).
    QTimer* m_pSetTimeTimer;
    // Last text shown in labelTangoSetTime, so the per-second tick only repaints
    // the label when the value actually changed (avoids needless toolbar repaints
    // that can flicker sibling widgets such as the waveform).
    QString m_lastSetTimeText;
    // Last text shown in labelEndTimeDelta, same repaint-avoidance rationale.
    QString m_lastEndTimeDeltaText;

    // When the current set started running. The target end time is resolved to
    // its first occurrence at or after this instant, so "midnight" set during a
    // morning soundcheck means the coming midnight, not the one already past.
    // Invalid while Auto DJ is stopped.
    QDateTime m_setStartDateTime;

    QString m_enableBtnTooltip;
    QString m_disableBtnTooltip;
};
