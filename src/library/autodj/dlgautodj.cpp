#include "library/autodj/dlgautodj.h"

#include <algorithm>
#include <cmath>

#include <QDateTime>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTimeEdit>
#include <QTimer>

#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/autodj/autodjfeature.h"
#include "library/autodj/cortinaregistry.h"
#include "library/autodj/tandaqueuemodel.h"
#include "library/autodj/wtandaqueueview.h"
#include "library/library.h"
#include "library/playlisttablemodel.h"
#include "mixer/playerinfo.h"
#include "moc_dlgautodj.cpp"
#include "track/track.h"
#include "util/assert.h"
#include "util/duration.h"
#include "widget/wcountdownoverlay.h"
#include "widget/wlibrary.h"
#include "widget/wtracktableview.h"

namespace {
const char* kPreferenceGroupName = "[Auto DJ]";
const char* kRepeatPlaylistPreference = "Requeue";
const char* kEndTimePreference = "TangoEndTime";
const char* kTandaGapPreference = "TandaGap";
constexpr int kDefaultTandaGapSeconds = 3;
const QString kDefaultEndTime = QStringLiteral("23:30:00");
const QString kEndTimeFormat = QStringLiteral("HH:mm:ss");

void setEnabledIfChanged(QWidget* pWidget, bool enabled) {
    if (pWidget && pWidget->isEnabled() != enabled) {
        pWidget->setEnabled(enabled);
    }
}

// Formats a set duration as HH:MM:SS, e.g. "2:03:47" or "0:47:12".
QString formatSetDuration(const mixxx::Duration& duration) {
    const qint64 totalSeconds = duration.toIntegerSeconds();
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
}

} // anonymous namespace

DlgAutoDJ::DlgAutoDJ(WLibrary* parent,
        UserSettingsPointer pConfig,
        Library* pLibrary,
        AutoDJFeature* pAutoDJFeature,
        AutoDJProcessor* pProcessor,
        KeyboardEventFilter* pKeyboard)
        : QWidget(parent),
          Ui::DlgAutoDJ(),
          m_pConfig(pConfig),
          m_pAutoDJProcessor(pProcessor),
          m_pTrackTableView(new WTandaQueueView(this,
                  m_pConfig,
                  pLibrary,
                  parent->getTrackTableBackgroundColorOpacity(),
                  pAutoDJFeature)),
          m_bShowButtonText(parent->getShowButtonText()),
          m_pAutoDJTableModel(nullptr),
          m_pTandaQueueModel(nullptr),
          m_pKeepQueueControl(nullptr),
          m_pCortinaLengthControl(nullptr),
          m_pKeyboard(pKeyboard),
          m_pLiveModeControl(nullptr),
          m_pStopCountdown(nullptr),
          m_pSetTimeTimer(nullptr) {
    setupUi(this);

    m_pTrackTableView->installEventFilter(pKeyboard);

    connect(m_pTrackTableView,
            &WTrackTableView::loadTrack,
            this,
            &DlgAutoDJ::loadTrack);
    connect(m_pTrackTableView,
            &WTrackTableView::loadTrackToPlayer,
            this,
            &DlgAutoDJ::loadTrackToPlayer);
    connect(m_pTrackTableView,
            &WTrackTableView::trackSelected,
            this,
            &DlgAutoDJ::trackSelected);
    connect(m_pTrackTableView,
            &WTrackTableView::trackSelected,
            this,
            &DlgAutoDJ::updateSelectionInfo);
    // Tagging the selected track as a cortina (or back to a track) changes what
    // its selected time should read, so re-total on cortina mark changes.
    connect(&CortinaRegistry::instance(),
            &CortinaRegistry::cortinaMarksChanged,
            this,
            &DlgAutoDJ::updateSelectionInfo);

    connect(pLibrary,
            &Library::setTrackTableFont,
            m_pTrackTableView,
            &WTrackTableView::setTrackTableFont);
    connect(pLibrary,
            &Library::setTrackTableRowHeight,
            m_pTrackTableView,
            &WTrackTableView::setTrackTableRowHeight);
    connect(pLibrary,
            &Library::setSelectedClick,
            m_pTrackTableView,
            &WTrackTableView::setSelectedClick);

    QBoxLayout* box = qobject_cast<QBoxLayout*>(layout());
    VERIFY_OR_DEBUG_ASSERT(box) { // Assumes the form layout is a QVBox/QHBoxLayout!
    }
    else {
        box->removeWidget(m_pTrackTablePlaceholder);
        m_pTrackTablePlaceholder->hide();
        box->insertWidget(1, m_pTrackTableView);
    }

    // We do _NOT_ take ownership of this from AutoDJProcessor.
    m_pAutoDJTableModel = m_pAutoDJProcessor->getTableModel();
    m_pTandaQueueModel = new TandaQueueModel(m_pAutoDJTableModel,
            pAutoDJFeature->tandaQueueState(),
            m_pAutoDJProcessor,
            this);
    // The Tango cortina styling (blue + "[--CORTINA--]" prefix) belongs to the
    // Auto DJ list only, and only while Tango mode is on - refreshTangoModeUi()
    // keeps it in step from here on, so outside Tango the list is stock Mixxx.
    m_pAutoDJTableModel->setShowCortinaMarks(false);
    m_pTrackTableView->loadTrackModel(m_pAutoDJTableModel);

    // Do not set this because it disables auto-scrolling
    // m_pTrackTableView->setDragDropMode(QAbstractItemView::InternalMove);

    connect(pushButtonAutoDJ,
            &QPushButton::clicked,
            this,
            &DlgAutoDJ::toggleAutoDJButton);

    setupActionButton(pushButtonFadeNow, &DlgAutoDJ::fadeNowButton, tr("Fade"));
    setupActionButton(pushButtonSkipNext, &DlgAutoDJ::skipNextButton, tr("Skip"));
    setupActionButton(pushButtonShuffle, &DlgAutoDJ::shufflePlaylistButton, tr("Shuffle"));
    setupActionButton(pushButtonAddRandomTrack, &DlgAutoDJ::addRandomTrackButton, tr("Random"));

    m_enableBtnTooltip = tr(
            "Enable Auto DJ\n"
            "\n"
            "Shortcut: Shift+F12");
    m_disableBtnTooltip = tr(
            "Disable Auto DJ\n"
            "\n"
            "Shortcut: Shift+F12");
    QString fadeBtnTooltip = tr(
            "Trigger the transition to the next track\n"
            "\n"
            "Shortcut: Shift+F11");
    QString skipBtnTooltip = tr(
            "Skip the next track in the Auto DJ queue\n"
            "\n"
            "Shortcut: Shift+F10");
    QString shuffleBtnTooltip = tr(
            "Shuffle the content of the Auto DJ queue\n"
            "\n"
            "Shortcut: Shift+F9");
    QString addRandomTrackBtnTooltip = tr(
            "Adds a random track from track sources (crates) to the Auto DJ queue.\n"
            "If no track sources are configured, the track is added from the library instead.");
    QString repeatBtnTooltip = tr(
            "Repeat the playlist");
    QString keepQueueBtnTooltip = tr(
            "Tango DJ mode (indicator).\n"
            "Plays the Auto DJ list in order, keeps played tracks, and stops at\n"
            "the end. Enable it in Preferences -> Auto DJ.");
    QString spinBoxTransitionTooltip = tr(
            "Determines the duration of the transition");
    QString labelTransitionTooltip = tr(
            // "sec" as in seconds
            "Seconds");
    QString fadeModeTooltip = tr(
            "Auto DJ Fade Modes\n"
            "\n"
            "Full Intro + Outro:\n"
            "Play the full intro and outro. Use the intro or outro length as the\n"
            "crossfade time, whichever is shorter. If no intro or outro are marked,\n"
            "use the selected crossfade time.\n"
            "\n"
            "Fade At Outro Start:\n"
            "Start crossfading at the outro start. If the outro is longer than the\n"
            "intro, cut off the end of the outro. Use the intro or outro length as\n"
            "the crossfade time, whichever is shorter. If no intro or outro are\n"
            "marked, use the selected crossfade time.\n"
            "\n"
            "Full Track:\n"
            "Play the whole track. Begin crossfading from the selected number of\n"
            "seconds before the end of the track. A negative crossfade time adds\n"
            "silence between tracks.\n"
            "\n"
            "Skip Silence:\n"
            "Play the whole track except for silence at the beginning and end.\n"
            "Begin crossfading from the selected number of seconds before the\n"
            "last sound.\n"
            "\n"
            "Tanda Transition:\n"
            "Play each track in full, then insert a fixed silent gap before the\n"
            "next track instead of crossfading. Set the gap length with the\n"
            "adjacent field. Only available while Tango DJ mode is on.");

    pushButtonFadeNow->setToolTip(fadeBtnTooltip);
    pushButtonSkipNext->setToolTip(skipBtnTooltip);
    pushButtonShuffle->setToolTip(shuffleBtnTooltip);
    pushButtonAddRandomTrack->setToolTip(addRandomTrackBtnTooltip);
    pushButtonRepeatPlaylist->setToolTip(repeatBtnTooltip);
    pushButtonKeepQueue->setToolTip(keepQueueBtnTooltip);
    spinBoxTransition->setToolTip(spinBoxTransitionTooltip);
    spinBoxTandaGap->setToolTip(tr("Silent gap inserted by Tanda Transition, in seconds. Positive values mean silence between tracks."));
    labelTransitionAppendix->setToolTip(labelTransitionTooltip);
    labelTandaGapAppendix->setToolTip(spinBoxTandaGap->toolTip());
    fadeModeCombobox->setToolTip(fadeModeTooltip);

    // Prevent the interactive widgets from being focused with Tab or Shift+Tab
    fadeModeCombobox->setFocusPolicy(Qt::ClickFocus);
    spinBoxTransition->setFocusPolicy(Qt::ClickFocus);
    spinBoxTandaGap->setFocusPolicy(Qt::ClickFocus);
    fadeModeCombobox->installEventFilter(pKeyboard);
    spinBoxTransition->installEventFilter(pKeyboard);
    spinBoxTandaGap->installEventFilter(pKeyboard);
    // work around QLineEdit being protected
    QLineEdit* lineEditTransition(spinBoxTransition->findChild<QLineEdit*>());
    lineEditTransition->setFocusPolicy(Qt::ClickFocus);
    // Needed to catch Enter, Return and Escape keypresses
    lineEditTransition->installEventFilter(this);
    lineEditTransition->installEventFilter(pKeyboard);
    QLineEdit* lineEditTandaGap(spinBoxTandaGap->findChild<QLineEdit*>());
    lineEditTandaGap->setFocusPolicy(Qt::ClickFocus);
    lineEditTandaGap->installEventFilter(this);
    lineEditTandaGap->installEventFilter(pKeyboard);

    spinBoxTandaGap->setValue(m_pConfig->getValue(
            ConfigKey(kPreferenceGroupName, kTandaGapPreference),
            kDefaultTandaGapSeconds));

    connect(spinBoxTransition,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgAutoDJ::transitionSliderChanged);
    connect(spinBoxTandaGap,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgAutoDJ::tandaGapChanged);

    fadeModeCombobox->addItem(tr("Full Intro + Outro"),
            static_cast<int>(AutoDJProcessor::TransitionMode::FullIntroOutro));
    fadeModeCombobox->addItem(tr("Fade At Outro Start"),
            static_cast<int>(AutoDJProcessor::TransitionMode::FadeAtOutroStart));
    fadeModeCombobox->addItem(tr("Full Track"),
            static_cast<int>(AutoDJProcessor::TransitionMode::FixedFullTrack));
    fadeModeCombobox->addItem(tr("Skip Silence"),
            static_cast<int>(AutoDJProcessor::TransitionMode::FixedSkipSilence));
    int transitionModeIndex = fadeModeCombobox->findData(
            static_cast<int>(m_pAutoDJProcessor->getTransitionMode()));
    if (transitionModeIndex < 0) {
        transitionModeIndex = fadeModeCombobox->findData(
                static_cast<int>(AutoDJProcessor::TransitionMode::FixedSkipSilence));
    }
    fadeModeCombobox->setCurrentIndex(transitionModeIndex);
    connect(fadeModeCombobox,
            QOverload<int>::of(&QComboBox::activated),
            this,
            &DlgAutoDJ::slotTransitionModeChanged);
    refreshTransitionControls();

    connect(pushButtonRepeatPlaylist,
            &QPushButton::clicked,
            this,
            &DlgAutoDJ::slotRepeatPlaylistChanged);
    if (m_bShowButtonText) {
        pushButtonRepeatPlaylist->setText(tr("Repeat"));
    }
    bool repeatPlaylist = m_pConfig->getValue<bool>(
            ConfigKey(kPreferenceGroupName, kRepeatPlaylistPreference));
    pushButtonRepeatPlaylist->setChecked(repeatPlaylist);
    slotRepeatPlaylistChanged(repeatPlaylist);

    // The Keep Queue button is a read-only indicator of Tango DJ mode, which is
    // enabled from Preferences -> Auto DJ. Make it ignore mouse clicks; its
    // state is refreshed by refreshTangoModeUi() (called from autoDJStateChanged
    // at the end of this constructor, after the transition spin box is set up).
    pushButtonKeepQueue->setAttribute(Qt::WA_TransparentForMouseEvents);
    if (m_bShowButtonText) {
        pushButtonKeepQueue->setText(tr("Tango"));
    }
    // Observe the Tango DJ mode control so the toolbar refreshes immediately
    // when it is toggled in Preferences -> Auto DJ.
    m_pKeepQueueControl = new ControlProxy(
            ConfigKey("[AutoDJ]", "keep_queue"), this);
    m_pKeepQueueControl->connectValueChanged(this, [this](double) {
        refreshTangoModeUi();
    });

    // Cockpit cortina-length nudge (Tango DJ mode). The − / + buttons bump the
    // shared [AutoDJ],cortina_length control by the nudge step; the value label
    // mirrors it and also reflects changes made in Preferences.
    m_pCortinaLengthControl = new ControlProxy(
            ConfigKey("[AutoDJ]", "cortina_length"), this);
    m_pCortinaLengthControl->connectValueChanged(this, [this](double) {
        updateCortinaLengthReadout();
        // A selected cortina counts as its length, so re-total the selection when
        // the length is nudged.
        updateSelectionInfo();
    });
    // Live-editable nudge step (1..10 s). Kept in the cockpit (not the stop-only
    // Preferences) so it can be tuned during a set; persisted so it is remembered.
    spinBoxCortinaNudgeStep->setValue(
            m_pConfig->getValue(ConfigKey("[Auto DJ]", "CortinaNudgeStep"), 2));
    connect(spinBoxCortinaNudgeStep,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int step) {
                m_pConfig->setValue(
                        ConfigKey("[Auto DJ]", "CortinaNudgeStep"), step);
            });
    connect(pushButtonCortinaLengthDown, &QPushButton::clicked, this, [this]() {
        nudgeCortinaLength(-spinBoxCortinaNudgeStep->value());
    });
    connect(pushButtonCortinaLengthUp, &QPushButton::clicked, this, [this]() {
        nudgeCortinaLength(spinBoxCortinaNudgeStep->value());
    });
    updateCortinaLengthReadout();

    // LIVE mode: a session-only performance lock. The toolbar shows a read-only
    // "LIVE" indicator (red when on); it is toggled deliberately via the
    // indicator's right-click menu so it can't be flipped by accident.
    m_pLiveModeControl = new ControlProxy(
            ConfigKey("[AutoDJ]", "live_mode"), this);
    m_pLiveModeControl->connectValueChanged(this, [this](double) {
        refreshLiveMode();
    });
    // Build the spacing into the label (gap before it, gap to the window edge)
    // rather than separate spacers, so hiding it when Tango is off leaves no gap.
    labelLive->setContextMenuPolicy(Qt::CustomContextMenu);
    labelLive->setContentsMargins(12, 0, 50, 0);
    connect(labelLive,
            &QLabel::customContextMenuRequested,
            this,
            &DlgAutoDJ::showLiveContextMenu);
    // Pie-countdown overlay shown on the Auto DJ button while the LIVE-mode stop
    // guard is armed. Parented to the button so it tracks its position/size and
    // is transparent to clicks so the button can still be pressed to confirm.
    m_pStopCountdown = new WCountdownOverlay(pushButtonAutoDJ);
    connect(m_pAutoDJProcessor,
            &AutoDJProcessor::stopGuardArmedChanged,
            this,
            &DlgAutoDJ::slotStopGuardArmedChanged);
    // Highlight the currently playing track (in red) in Tango DJ mode.
    connect(&PlayerInfo::instance(),
            &PlayerInfo::currentPlayingTrackChanged,
            this,
            [this](TrackPointer) {
                updateNowPlaying();
                refreshTangoModeUi();
            });

    // Keep the Tango set end-time readout ticking so the projected end clock
    // slips while paused and the time-left counts down between track changes. The
    // timer is only started while Tango mode is on (refreshTangoModeUi), so it
    // does no per-second work when the feature is off.
    m_pSetTimeTimer = new QTimer(this);
    m_pSetTimeTimer->setInterval(1000);
    connect(m_pSetTimeTimer, &QTimer::timeout, this, &DlgAutoDJ::updateSetEndTime);

    // Target end time for the milonga (Tango DJ mode). The over/under indicator
    // next to it compares the projected set end against this. Editable at any time
    // and persisted; defaults to 23:30:00.
    QTime endTime = QTime::fromString(
            m_pConfig->getValue(ConfigKey(kPreferenceGroupName, kEndTimePreference),
                    kDefaultEndTime),
            kEndTimeFormat);
    if (!endTime.isValid()) {
        endTime = QTime::fromString(kDefaultEndTime, kEndTimeFormat);
    }
    endTimeEdit->setTime(endTime);
    connect(endTimeEdit,
            &QTimeEdit::timeChanged,
            this,
            &DlgAutoDJ::slotEndTimeChanged);

    // Setup DlgAutoDJ UI based on the current AutoDJProcessor state. Keep in
    // mind that AutoDJ may already be active when DlgAutoDJ is created (due to
    // skin changes, etc.).
    spinBoxTransition->setValue(static_cast<int>(m_pAutoDJProcessor->getTransitionTime()));
    connect(m_pAutoDJProcessor,
            &AutoDJProcessor::transitionTimeChanged,
            this,
            &DlgAutoDJ::transitionTimeChanged);

    connect(m_pAutoDJProcessor,
            &AutoDJProcessor::autoDJError,
            this,
            &DlgAutoDJ::autoDJError);

    connect(m_pAutoDJProcessor,
            &AutoDJProcessor::autoDJStateChanged,
            this,
            &DlgAutoDJ::autoDJStateChanged);
    autoDJStateChanged(m_pAutoDJProcessor->getState());

    updateSelectionInfo();
    refreshLiveMode();
}

DlgAutoDJ::~DlgAutoDJ() {
    qDebug() << "~DlgAutoDJ()";

    // Release the LIVE-mode key suppression so it doesn't outlive this view (a new
    // DlgAutoDJ re-applies it from refreshLiveMode() if LIVE is still on).
    if (m_pKeyboard) {
        m_pKeyboard->setControlSuppressed(ConfigKey("[Channel1]", "play"), false);
        m_pKeyboard->setControlSuppressed(ConfigKey("[Channel2]", "play"), false);
    }

    // Delete m_pTrackTableView before the table model. This is because the
    // table view saves the header state using the model.
    delete m_pTrackTableView;
}

void DlgAutoDJ::setupActionButton(QPushButton* pButton,
        void (DlgAutoDJ::*pSlot)(bool),
        const QString& fallbackText) {
    connect(pButton, &QPushButton::clicked, this, pSlot);
    if (m_bShowButtonText) {
        pButton->setText(fallbackText);
    }
}

void DlgAutoDJ::onShow() {
    m_pAutoDJTableModel->select();
    // Tango DJ mode may have been toggled in Preferences while this view was
    // hidden, so refresh the indicator and the locked controls.
    refreshTangoModeUi();
}

void DlgAutoDJ::onSearch(const QString& text) {
    // Do not allow filtering the Auto DJ playlist, because
    // Auto DJ will work from the filtered table
    Q_UNUSED(text);
}

void DlgAutoDJ::shufflePlaylistButton(bool) {
    QModelIndexList indexList = m_pTrackTableView->selectionModel()->selectedRows();

    // Activate regardless of button being checked
    m_pAutoDJProcessor->shufflePlaylist(indexList);
}

void DlgAutoDJ::skipNextButton(bool) {
    // Activate regardless of button being checked
    m_pAutoDJProcessor->skipNext();
}

void DlgAutoDJ::fadeNowButton(bool) {
    // Activate regardless of button being checked
    const bool tango = m_pKeepQueueControl && m_pKeepQueueControl->toBool();
    if (!tango) {
        m_pAutoDJProcessor->fadeNow();
        return;
    }

    const bool canFade = m_pAutoDJProcessor->canFadePlayingCortinaNow();
    qInfo().nospace() << "[CORTINA_BUTTON] clicked enabled="
                      << (pushButtonFadeNow->isEnabled() ? "yes" : "no")
                      << " visible="
                      << (pushButtonFadeNow->isVisible() ? "yes" : "no")
                      << " tango=" << (tango ? "yes" : "no")
                      << " canFade=" << (canFade ? "yes" : "no")
                      << " state=" << static_cast<int>(m_pAutoDJProcessor->getState());
    const bool faded = m_pAutoDJProcessor->fadePlayingCortinaNow();
    qInfo().nospace() << "[CORTINA_BUTTON] processorResult="
                      << (faded ? "yes" : "no");
    refreshTangoModeUi();
}

void DlgAutoDJ::toggleAutoDJButton(bool enable) {
    m_pAutoDJProcessor->toggleAutoDJ(enable);
}

// TODO If there's a way to migrate the translations move this
// to AutoDJProcessor in order to keep this class minimal
void DlgAutoDJ::autoDJError(AutoDJProcessor::AutoDJError error) {
    switch (error) {
    case AutoDJProcessor::ADJ_NOT_TWO_DECKS:
        QMessageBox::warning(nullptr,
                tr("Auto DJ"),
                tr("Auto DJ requires two decks assigned to opposite sides of the crossfader."),
                QMessageBox::Ok);
        break;
    case AutoDJProcessor::ADJ_BOTH_DECKS_PLAYING:
        QMessageBox::warning(nullptr,
                tr("Auto DJ"),
                tr("One deck must be stopped to enable Auto DJ mode."),
                QMessageBox::Ok);
        break;
    case AutoDJProcessor::ADJ_UNUSED_DECK_PLAYING:
        QMessageBox::warning(nullptr,
                tr("Auto DJ"),
                tr("Decks not used for Auto DJ must be stopped to enable Auto DJ mode."),
                QMessageBox::Ok);
        break;
    case AutoDJProcessor::ADJ_OK:
    default:
        break;
    }
}

void DlgAutoDJ::transitionTimeChanged(int time) {
    spinBoxTransition->setValue(time);
}

void DlgAutoDJ::transitionSliderChanged(int value) {
    m_pAutoDJProcessor->setTransitionTime(value);
}

void DlgAutoDJ::tandaGapChanged(int value) {
    m_pAutoDJProcessor->setTandaGapSeconds(value);
}

void DlgAutoDJ::autoDJStateChanged(AutoDJProcessor::AutoDJState state) {
    if (state == AutoDJProcessor::ADJ_DISABLED) {
        pushButtonAutoDJ->setChecked(false);
        pushButtonAutoDJ->setToolTip(m_enableBtnTooltip);
        if (m_bShowButtonText) {
            pushButtonAutoDJ->setText(tr("Enable"));
        }
    } else {
        // No matter the mode, you can always disable once it is enabled.
        pushButtonAutoDJ->setChecked(true);
        pushButtonAutoDJ->setToolTip(m_disableBtnTooltip);
        if (m_bShowButtonText) {
            pushButtonAutoDJ->setText(tr("Disable"));
        }
    }
    // Skip / Fade Now availability and the locked-control state depend on both
    // the Auto DJ state and Tango mode.
    refreshTangoModeUi();
}

void DlgAutoDJ::slotTransitionModeChanged(int newIndex) {
    m_pAutoDJProcessor->setTransitionMode(
            static_cast<AutoDJProcessor::TransitionMode>(
                    fadeModeCombobox->itemData(newIndex).toInt()));
    refreshTransitionControls();
    // Clicking on a transition mode item moves keyboard focus to the list widget.
    // Move focus back to the previously focused library widget.
    ControlObject::set(ConfigKey("[Library]", "refocus_prev_widget"), 1);
}

void DlgAutoDJ::slotRepeatPlaylistChanged(bool checked) {
    m_pConfig->setValue(ConfigKey(kPreferenceGroupName, kRepeatPlaylistPreference),
            checked);
}

void DlgAutoDJ::refreshTangoModeUi() {
    const bool tango = m_pKeepQueueControl->toBool();
    QAbstractItemModel* pDesiredModel =
            tango ? static_cast<QAbstractItemModel*>(m_pTandaQueueModel)
                  : static_cast<QAbstractItemModel*>(m_pAutoDJTableModel);
    if (m_pTrackTableView->model() != pDesiredModel) {
        m_pTrackTableView->loadTrackModel(pDesiredModel, true);
    }
    // Update the read-only toolbar indicator. It is hidden outside Tango mode
    // rather than shown in an "off" state, so plain Mixxx carries no trace of
    // the fork's Tango UI.
    pushButtonKeepQueue->setChecked(tango);
    pushButtonKeepQueue->setVisible(tango);
    // Cortina marks follow Tango mode, matching the deck's mark. This also scopes
    // the "mark as cortina" action, which WTrackMenu derives from the same flag:
    // tagging only means anything with Cortina Fade, which is Tango-only. The
    // marks themselves live in CortinaRegistry and are untouched - leaving Tango
    // hides them, and they are all still there on return.
    if (m_pAutoDJTableModel) {
        m_pAutoDJTableModel->setShowCortinaMarks(tango);
    }
    // The set-time readout, the target end-time controls and the LIVE indicator
    // are Tango-only.
    labelSetEndTime->setVisible(tango);
    endTimeEdit->setVisible(tango);
    labelEndTimeDelta->setVisible(tango);
    labelCortinaLengthCaption->setVisible(tango);
    pushButtonCortinaLengthDown->setVisible(tango);
    labelCortinaLengthValue->setVisible(tango);
    pushButtonCortinaLengthUp->setVisible(tango);
    spinBoxCortinaNudgeStep->setVisible(tango);
    labelLive->setVisible(tango);
    if (tango) {
        updateCortinaLengthReadout();
    }
    // LIVE mode only exists within Tango mode: leaving Tango exits LIVE so its
    // guards (stop-confirm, deck-key suppression) can't linger outside Tango.
    if (!tango && m_pLiveModeControl && m_pLiveModeControl->toBool()) {
        m_pLiveModeControl->set(0.0);
    }
    refreshLiveMode();
    // In Tango mode these controls are hidden: a pre-arranged live set is never
    // shuffled, randomised or repeated by hand, so they only add clutter and a
    // chance to wreck the set by accident. Hiding (rather than dimming) keeps the
    // toolbar clean during a milonga; outside Tango they return as stock Mixxx.
    for (QPushButton* pButton : {pushButtonShuffle,
                 pushButtonAddRandomTrack,
                 pushButtonRepeatPlaylist}) {
        pButton->setVisible(!tango);
        pButton->setEnabled(!tango);
    }
    // The stock Fade Now button becomes Fade Cortina in Tango mode. Reusing the
    // same widget preserves the skin's native hover, press, and disabled states.
    const AutoDJProcessor::AutoDJState state = m_pAutoDJProcessor->getState();
    const bool running = state != AutoDJProcessor::ADJ_DISABLED;
    const bool fading = state == AutoDJProcessor::ADJ_LEFT_FADING ||
            state == AutoDJProcessor::ADJ_RIGHT_FADING ||
            state == AutoDJProcessor::ADJ_ENABLE_P1LOADED;
    // Skip is hidden in Tango mode too (Auto DJ advances tandas on its own), and
    // returns to its stock running-dependent enabled state outside Tango.
    pushButtonSkipNext->setVisible(!tango);
    pushButtonSkipNext->setEnabled(running && !tango);
    setEnabledIfChanged(pushButtonFadeNow,
            tango ? m_pAutoDJProcessor->canFadePlayingCortinaNow()
                  : running && !fading);
    pushButtonFadeNow->setToolTip(tango
                    ? tr("Fade out the currently playing cortina now, then "
                         "continue with the configured gap and next tanda track.")
                    : tr("Trigger the transition to the next track\n\n"
                         "Shortcut: Shift+F11"));
    if (m_bShowButtonText) {
        pushButtonFadeNow->setText(tango ? tr("Fade Cortina") : tr("Fade"));
    }
    // Prevent click-to-sort from reordering the queue out of its play order.
    QHeaderView* pHeader = m_pTrackTableView->horizontalHeader();
    pHeader->setSectionsClickable(!tango);
    pHeader->setSortIndicatorShown(!tango);
    refreshTransitionModeOptions();
    syncTransitionModeFromProcessor();
    // Only run the per-second set-time tick while Tango mode is on.
    if (tango) {
        if (!m_pSetTimeTimer->isActive()) {
            m_pSetTimeTimer->start();
        }
    } else {
        m_pSetTimeTimer->stop();
    }
    updateNowPlaying();
    updateSetEndTime();
}

void DlgAutoDJ::nudgeCortinaLength(int delta) {
    const int current = static_cast<int>(std::lround(m_pCortinaLengthControl->get()));
    const int next = std::clamp(current + delta, 5, 600);
    if (next != current) {
        m_pCortinaLengthControl->set(next);
    }
    // The control does not echo its own set back to this proxy, so refresh the
    // readout and the selection total directly (a change from Preferences arrives
    // via connectValueChanged instead).
    updateCortinaLengthReadout();
    updateSelectionInfo();
}

void DlgAutoDJ::updateCortinaLengthReadout() {
    const int seconds = static_cast<int>(std::lround(m_pCortinaLengthControl->get()));
    labelCortinaLengthValue->setText(tr("%1 s").arg(seconds));
}

void DlgAutoDJ::updateNowPlaying() {
    // Only mark the now-playing track in Tango DJ mode.
    TrackPointer pTrack;
    if (m_pKeepQueueControl->toBool()) {
        pTrack = PlayerInfo::instance().getCurrentPlayingTrack();
    }
    m_pAutoDJTableModel->setNowPlayingTrack(pTrack ? pTrack->getId() : TrackId());
}

void DlgAutoDJ::updateSetEndTime() {
    QString text;
    QString deltaText;
    if (m_pKeepQueueControl && m_pKeepQueueControl->toBool()) {
        setEnabledIfChanged(pushButtonFadeNow,
                m_pAutoDJProcessor->canFadePlayingCortinaNow());
    }
    // The readout is a Tango DJ mode feature only; otherwise it stays empty.
    if (m_pKeepQueueControl && m_pKeepQueueControl->toBool()) {
        // "Set Length" is the constant total of the whole set; it reads the same
        // whether Auto DJ is running or not. The running state then *appends*
        // "Ends" and "Left" so the line never needs to be re-read - the off->on
        // change is purely additive, which keeps the cognitive load low live.
        const mixxx::Duration total = m_pAutoDJProcessor->getTotalSetDuration();
        // A non-positive total means the queue is empty / nothing to play.
        if (total.toIntegerMillis() > 0) {
            text = tr("Set Length: %1").arg(formatSetDuration(total));
            if (m_pAutoDJProcessor->getState() != AutoDJProcessor::ADJ_DISABLED) {
                const mixxx::Duration remaining =
                        m_pAutoDJProcessor->getRemainingSetDuration();
                // The projected end clock is the most important number, so
                // emphasise it in red.
                const QDateTime end = QDateTime::currentDateTime().addMSecs(
                        remaining.toIntegerMillis());
                const QString endRed = QStringLiteral(
                        "<span style=\"color:#ee4444; font-weight:bold;\">%1</span>")
                                               .arg(end.toString(QStringLiteral(
                                                       "HH:mm:ss")));
                // Non-breaking spaces so the rich-text label keeps the gaps.
                const QString gap = QStringLiteral("&nbsp;&nbsp;&nbsp;");
                text += gap + tr("Ends: %1").arg(endRed);
                text += gap + tr("Left: %1").arg(formatSetDuration(remaining));
                // Over/under against the target end time, shown only while running
                // (there is no projected end clock otherwise).
                deltaText = formatEndTimeDelta(end);
            }
        }
    }
    // Only touch the labels when the text actually changes. Re-setting them every
    // second otherwise forces a needless toolbar repaint, which can make sibling
    // widgets (e.g. the deck waveforms) flicker.
    if (text != m_lastSetTimeText) {
        m_lastSetTimeText = text;
        labelTangoSetTime->setText(text);
    }
    if (deltaText != m_lastEndTimeDeltaText) {
        m_lastEndTimeDeltaText = deltaText;
        labelEndTimeDelta->setText(deltaText);
    }
}

QString DlgAutoDJ::formatEndTimeDelta(const QDateTime& projectedEnd) const {
    // Anchor the target time-of-day to the calendar day nearest the projected end
    // so the comparison stays correct across midnight (e.g. target 00:15 vs an end
    // clock of 23:58). Start from the projected end (same date and time zone) and
    // overwrite just the time of day.
    QDateTime target = projectedEnd;
    target.setTime(endTimeEdit->time());
    constexpr qint64 kHalfDaySecs = 12 * 3600;
    const qint64 toEnd = target.secsTo(projectedEnd);
    if (toEnd > kHalfDaySecs) {
        target = target.addDays(1);
    } else if (toEnd < -kHalfDaySecs) {
        target = target.addDays(-1);
    }
    const qint64 deltaSecs = target.secsTo(projectedEnd);
    if (deltaSecs == 0) {
        return tr("● on time");
    }
    // Positive => the set ends after the target (running over); negative => under.
    const bool over = deltaSecs > 0;
    const QString magnitude = formatSetDuration(
            mixxx::Duration::fromMillis(qAbs(deltaSecs) * 1000));
    return QStringLiteral("<span style=\"color:%1; font-weight:bold;\">%2 %3%4 %5</span>")
            .arg(over ? QStringLiteral("#ee4444") : QStringLiteral("#55aa55"),
                    over ? QStringLiteral("▲") : QStringLiteral("▼"),
                    over ? QStringLiteral("+") : QStringLiteral("-"),
                    magnitude,
                    over ? tr("over") : tr("under"));
}

void DlgAutoDJ::slotEndTimeChanged(const QTime& time) {
    m_pConfig->setValue(ConfigKey(kPreferenceGroupName, kEndTimePreference),
            time.toString(kEndTimeFormat));
    // Refresh immediately so the over/under indicator tracks the edit without
    // waiting for the next per-second tick.
    updateSetEndTime();
}

void DlgAutoDJ::refreshLiveMode() {
    // LIVE is only meaningful in Tango mode; treat it as off otherwise.
    const bool live = m_pLiveModeControl && m_pLiveModeControl->toBool() &&
            m_pKeepQueueControl && m_pKeepQueueControl->toBool();
    // Indicator: bold red "LIVE" when on, greyed (disabled-looking) when off. The
    // label stays enabled either way so its right-click menu keeps working.
    if (live) {
        labelLive->setText(QStringLiteral(
                "<span style=\"color:#ee2222; font-weight:bold;\">LIVE</span>"));
    } else {
        labelLive->setText(QStringLiteral(
                "<span style=\"color:#666666; font-weight:bold;\">LIVE</span>"));
    }
    // Make the deck play/pause keys (D = [Channel1],play, L = [Channel2],play)
    // inert while LIVE, so an accidental press can't stop the playing deck.
    if (m_pKeyboard) {
        m_pKeyboard->setControlSuppressed(ConfigKey("[Channel1]", "play"), live);
        m_pKeyboard->setControlSuppressed(ConfigKey("[Channel2]", "play"), live);
    }
}

void DlgAutoDJ::showLiveContextMenu(const QPoint& pos) {
    if (!m_pLiveModeControl) {
        return;
    }
    const bool live = m_pLiveModeControl->toBool();
    QMenu menu(this);
    // Tag with an object name so the skin stylesheet themes it like other menus
    // instead of falling back to native styling.
    menu.setObjectName(QStringLiteral("AutoDJContextMenu"));
    QAction* pAction = menu.addAction(live ? tr("Exit LIVE mode") : tr("Enter LIVE mode"));
    if (menu.exec(labelLive->mapToGlobal(pos)) == pAction) {
        m_pLiveModeControl->set(live ? 0.0 : 1.0);
        // Update the indicator and key suppression immediately, in case the proxy
        // does not deliver its own set back as a valueChanged.
        refreshLiveMode();
    }
}

void DlgAutoDJ::slotStopGuardArmedChanged(bool armed) {
    // The Auto DJ icon is a power symbol (left) + android (right) = "Auto DJ".
    // While armed, a depleting red pie sits over just the power symbol (its
    // opaque backdrop masks it); the android stays visible. The eaten part grows
    // until the pie is empty when the window expires. The overlay ignores mouse
    // events, so a second press on the button confirms the stop.
    if (armed) {
        pushButtonAutoDJ->setChecked(true);
        pushButtonAutoDJ->setToolTip(
                tr("Press again to stop the set (LIVE mode)."));
        if (m_pStopCountdown) {
            // The button is the container: it fills solid red, then the red level
            // falls to empty over the guard window, revealing the icon beneath.
            // Constrain the overlay to the button's *visible* area (inside the QSS
            // margins/border) so it stays within the button instead of spilling
            // into the margin; the box model differs per skin, so use the style.
            QStyleOptionButton option;
            option.initFrom(pushButtonAutoDJ);
            QRect inner = pushButtonAutoDJ->style()->subElementRect(
                    QStyle::SE_PushButtonContents, &option, pushButtonAutoDJ);
            if (!inner.isValid() || inner.isEmpty()) {
                inner = pushButtonAutoDJ->rect();
            }
            // Grab the button area behind the liquid (the overlay is hidden, so it
            // is not in the grab) to use as the drained-part background.
            const QPixmap background = pushButtonAutoDJ->grab(inner);
            m_pStopCountdown->setGeometry(inner);
            // Matches the AutoDJProcessor stop-guard timer (3 s).
            m_pStopCountdown->start(3000, background);
        }
    } else {
        if (m_pStopCountdown) {
            m_pStopCountdown->stop();
        }
        // Restore the normal button appearance for the current Auto DJ state
        // (autoDJStateChanged resets the checked state and tooltip).
        autoDJStateChanged(m_pAutoDJProcessor->getState());
    }
}

void DlgAutoDJ::syncTransitionModeFromProcessor() {
    refreshTransitionModeOptions();
    const int index = fadeModeCombobox->findData(
            static_cast<int>(m_pAutoDJProcessor->getTransitionMode()));
    if (index >= 0 && fadeModeCombobox->currentIndex() != index) {
        fadeModeCombobox->setCurrentIndex(index);
    }
    refreshTransitionControls();
}

void DlgAutoDJ::refreshTransitionModeOptions() {
    const int tandaMode = static_cast<int>(
            AutoDJProcessor::TransitionMode::TandaTransition);
    const int tandaIndex = fadeModeCombobox->findData(tandaMode);
    const bool tango = m_pKeepQueueControl && m_pKeepQueueControl->toBool();
    if (tango && tandaIndex < 0) {
        fadeModeCombobox->addItem(tr("Tanda Transition"), tandaMode);
    } else if (!tango && tandaIndex >= 0) {
        fadeModeCombobox->removeItem(tandaIndex);
    }
}

void DlgAutoDJ::refreshTransitionControls() {
    const bool tandaTransition = static_cast<AutoDJProcessor::TransitionMode>(
            fadeModeCombobox->currentData().toInt()) ==
            AutoDJProcessor::TransitionMode::TandaTransition;
    spinBoxTransition->setVisible(!tandaTransition);
    labelTransitionAppendix->setVisible(!tandaTransition);
    spinBoxTandaGap->setVisible(tandaTransition);
    spinBoxTandaGap->setEnabled(tandaTransition);
    labelTandaGapAppendix->setVisible(tandaTransition);
    labelTandaGapAppendix->setEnabled(tandaTransition);
}

void DlgAutoDJ::updateSelectionInfo() {
    QModelIndexList indices = m_pTrackTableView->selectionModel()->selectedRows();

    const bool tangoList = m_pTrackTableView->model() == m_pTandaQueueModel;
    if (tangoList) {
        indices = m_pTandaQueueModel->mapSelectionToSource(indices);
    }

    double totalSeconds = 0.0;
    if (tangoList) {
        // A cortina only plays for its configured cortina length (clamped to the
        // file), not the whole file, so count that. Non-cortina tracks use their
        // full duration. (Durations are still whole-file, not LAS/FAS - a later
        // release may switch to the audible span.)
        // Read the length from the control the nudge buttons drive, so a live
        // nudge is reflected immediately without depending on the config write
        // ordering.
        const double cortinaLength = m_pCortinaLengthControl
                ? m_pCortinaLengthControl->get()
                : static_cast<double>(m_pConfig->getValue(
                          ConfigKey("[Auto DJ]", "CortinaLength"), 45));
        for (const QModelIndex& index : indices) {
            const double full =
                    m_pAutoDJTableModel->durationSecondsForRow(index.row());
            if (CortinaRegistry::instance().contains(
                        m_pAutoDJTableModel->getTrackId(index))) {
                totalSeconds += std::min(full, cortinaLength);
            } else {
                totalSeconds += full;
            }
        }
    } else {
        // Derive total duration from the table model. This is much faster than
        // getting the duration from individual track objects.
        totalSeconds =
                m_pAutoDJTableModel->getTotalDuration(indices).toDoubleSeconds();
    }

    QString label;

    if (!indices.isEmpty()) {
        label.append(mixxx::DurationBase::formatTime(totalSeconds));
        label.append(QString(" (%1)").arg(indices.size()));
        labelSelectionInfo->setToolTip(tr("Displays the duration and number of selected tracks."));
        labelSelectionInfo->setText(label);
        labelSelectionInfo->setEnabled(true);
    } else {
        labelSelectionInfo->setText("");
        labelSelectionInfo->setEnabled(false);
    }
}

bool DlgAutoDJ::hasFocus() const {
    return m_pTrackTableView->hasFocus();
}

void DlgAutoDJ::setFocus() {
    m_pTrackTableView->setFocus();
}

void DlgAutoDJ::pasteFromSidebar() {
    m_pTrackTableView->pasteFromSidebar();
}

void DlgAutoDJ::keyPressEvent(QKeyEvent* pEvent) {
    // If we receive key events either the mode selector or the spinbox are focused.
    // Return, Enter and Escape move focus back to the previously focused
    // library widget in order to immediately allow keyboard shortcuts again.
    if (pEvent->key() == Qt::Key_Return ||
            pEvent->key() == Qt::Key_Enter ||
            pEvent->key() == Qt::Key_Escape) {
        ControlObject::set(ConfigKey("[Library]", "refocus_prev_widget"), 1);
        return;
    }
    QWidget::keyPressEvent(pEvent);
}

void DlgAutoDJ::saveCurrentViewState() {
    m_pTrackTableView->saveCurrentViewState();
}

bool DlgAutoDJ::restoreCurrentViewState() {
    return m_pTrackTableView->restoreCurrentViewState();
}
