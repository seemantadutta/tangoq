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
class WLibrary;
class WTrackTableView;
class Library;
class KeyboardEventFilter;
class QTimer;

class DlgAutoDJ : public QWidget, public Ui::DlgAutoDJ, public LibraryView {
    Q_OBJECT
  public:
    DlgAutoDJ(WLibrary* parent,
            UserSettingsPointer pConfig,
            Library* pLibrary,
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
    // Applies the Tango fade/gap defaults (Skip Silence + short gap) the first
    // time, only if they are still at their factory defaults.
    void applyTangoDefaultsIfNeeded();
    // Marks the currently playing track (red) in the Auto DJ list in Tango mode.
    void updateNowPlaying();
    // Refreshes the Tango DJ mode set end-time / time-left readout, including the
    // over/under delta against the target end time.
    void updateSetEndTime();
    // Builds the colored over/under delta text comparing the projected end against
    // the target end time (endTimeEdit), e.g. "▲ +0:04:20 over".
    QString formatEndTimeDelta(const QDateTime& projectedEnd) const;
    void keyPressEvent(QKeyEvent* pEvent) override;

    const UserSettingsPointer m_pConfig;

    AutoDJProcessor* const m_pAutoDJProcessor;
    WTrackTableView* const m_pTrackTableView;
    const bool m_bShowButtonText;

    PlaylistTableModel* m_pAutoDJTableModel;

    // Observes [AutoDJ],keep_queue (Tango DJ mode) so the toolbar refreshes
    // immediately when it is toggled in Preferences.
    ControlProxy* m_pKeepQueueControl;

    // Ticks once a second to keep the Tango set end-time readout current. Only
    // runs while Tango mode is on (see refreshTangoModeUi).
    QTimer* m_pSetTimeTimer;
    // Last text shown in labelTangoSetTime, so the per-second tick only repaints
    // the label when the value actually changed (avoids needless toolbar repaints
    // that can flicker sibling widgets such as the waveform).
    QString m_lastSetTimeText;
    // Last text shown in labelEndTimeDelta, same repaint-avoidance rationale.
    QString m_lastEndTimeDeltaText;

    QString m_enableBtnTooltip;
    QString m_disableBtnTooltip;
};
