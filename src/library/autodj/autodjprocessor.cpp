#include "library/autodj/autodjprocessor.h"

#include <algorithm>
#include <cmath>

#include <QDebug>

#include "engine/channels/enginedeck.h"
#include "library/autodj/cortinaregistry.h"
#include "library/basetracktablemodel.h"
#include "library/columncache.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playerinfo.h"
#include "mixer/playermanager.h"
#include "moc_autodjprocessor.cpp"
#include "track/cue.h"
#include "track/cueinfo.h"
#include "track/globaltrackcache.h"
#include "track/track.h"
#include "track/tangostartcue.h"
#include "util/math.h"

namespace {
const QString kPreferenceGroup = QStringLiteral("[Auto DJ]");
const QString kControlGroup = QStringLiteral("[AutoDJ]");
const char* kTransitionPreferenceName = "Transition";
const char* kTransitionModePreferenceName = "TransitionMode";
const char* kLastStockTransitionModePreferenceName = "LastStockTransitionMode";
const char* kTandaGapPreferenceName = "TandaGap";
constexpr int kTandaGapPreferenceDefault = 3;
constexpr double kTransitionPreferenceDefault = 10.0;
constexpr double kKeepPosition = -1.0;

// A track needs to be longer than two callbacks to not stop AutoDJ
constexpr double kMinimumTrackDurationSec = 0.2;

constexpr bool sDebug = false;

const char* transitionModeName(AutoDJProcessor::TransitionMode mode) {
    switch (mode) {
    case AutoDJProcessor::TransitionMode::FullIntroOutro:
        return "FullIntroOutro";
    case AutoDJProcessor::TransitionMode::FadeAtOutroStart:
        return "FadeAtOutroStart";
    case AutoDJProcessor::TransitionMode::FixedFullTrack:
        return "FixedFullTrack";
    case AutoDJProcessor::TransitionMode::FixedSkipSilence:
        return "FixedSkipSilence";
    case AutoDJProcessor::TransitionMode::TandaTransition:
        return "TandaTransition";
    }
    return "Unknown";
}

const char* yesNo(bool value) {
    return value ? "yes" : "no";
}

bool isTandaTransition(AutoDJProcessor::TransitionMode mode) {
    return mode == AutoDJProcessor::TransitionMode::TandaTransition;
}

bool isStockTransitionMode(AutoDJProcessor::TransitionMode mode) {
    return !isTandaTransition(mode);
}

AutoDJProcessor::TransitionMode validStockTransitionMode(int mode) {
    switch (static_cast<AutoDJProcessor::TransitionMode>(mode)) {
    case AutoDJProcessor::TransitionMode::FullIntroOutro:
    case AutoDJProcessor::TransitionMode::FadeAtOutroStart:
    case AutoDJProcessor::TransitionMode::FixedFullTrack:
    case AutoDJProcessor::TransitionMode::FixedSkipSilence:
        return static_cast<AutoDJProcessor::TransitionMode>(mode);
    case AutoDJProcessor::TransitionMode::TandaTransition:
        break;
    }
    return AutoDJProcessor::TransitionMode::FixedSkipSilence;
}

#if defined(TANGO_TRANSITION_TRACE)
#define TT_TRACE() qInfo().nospace() << "[TT] "
#else
#define TT_TRACE() if constexpr (false) qInfo().nospace()
#endif
} // anonymous namespace

DeckAttributes::DeckAttributes(int index,
        BaseTrackPlayer* pPlayer)
        : index(index),
          group(pPlayer->getGroup()),
          startPos(kKeepPosition),
          fadeBeginPos(1.0),
          fadeEndPos(1.0),
          isFromDeck(false),
          loading(false),
          m_orientation(group, "orientation"),
          m_playPos(group, "playposition"),
          m_play(group, "play"),
          m_repeat(group, "repeat"),
          m_introStartPos(group, "intro_start_position"),
          m_introEndPos(group, "intro_end_position"),
          m_outroStartPos(group, "outro_start_position"),
          m_outroEndPos(group, "outro_end_position"),
          m_trackSamples(group, "track_samples"),
          m_sampleRate(group, "track_samplerate"),
          m_rateRatio(group, "rate_ratio"),
          m_autoDJFadeGain(group, "autodj_fade_gain"),
          m_pPlayer(pPlayer) {
    connect(m_pPlayer, &BaseTrackPlayer::newTrackLoaded,
            this, &DeckAttributes::slotTrackLoaded);
    connect(m_pPlayer, &BaseTrackPlayer::loadingTrack,
            this, &DeckAttributes::slotLoadingTrack);
    connect(m_pPlayer, &BaseTrackPlayer::playerEmpty,
            this, &DeckAttributes::slotPlayerEmpty);
    // PlayerManager owns the decks and is destroyed before the Library that owns
    // AutoDJProcessor, so this raw pointer outlives what it points at. Every
    // other accessor here goes through a ControlProxy, which keeps its control
    // alive independently; getLoadedTrack() is the one that dereferences the
    // player, and calling it on a freed deck jumps through a reused vtable.
    connect(m_pPlayer, &BaseTrackPlayer::destroyed, this, [this]() {
        m_pPlayer = nullptr;
    });
    m_playPos.connectValueChanged(this, &DeckAttributes::slotPlayPosChanged);
    m_play.connectValueChanged(this, &DeckAttributes::slotPlayChanged);
    m_introStartPos.connectValueChanged(this, &DeckAttributes::slotIntroStartPositionChanged);
    m_introEndPos.connectValueChanged(this, &DeckAttributes::slotIntroEndPositionChanged);
    m_outroStartPos.connectValueChanged(this, &DeckAttributes::slotOutroStartPositionChanged);
    m_outroEndPos.connectValueChanged(this, &DeckAttributes::slotOutroEndPositionChanged);
    m_rateRatio.connectValueChanged(this, &DeckAttributes::slotRateChanged);
}

DeckAttributes::~DeckAttributes() {
}

void DeckAttributes::slotPlayChanged(double v) {
    emit playChanged(this, v > 0.0);
}

void DeckAttributes::slotPlayPosChanged(double v) {
    emit playPositionChanged(this, v);
}

void DeckAttributes::slotIntroStartPositionChanged(double v) {
    emit introStartPositionChanged(this, v);
}

void DeckAttributes::slotIntroEndPositionChanged(double v) {
    emit introEndPositionChanged(this, v);
}

void DeckAttributes::slotOutroStartPositionChanged(double v) {
    emit outroStartPositionChanged(this, v);
}

void DeckAttributes::slotOutroEndPositionChanged(double v) {
    emit outroEndPositionChanged(this, v);
}

void DeckAttributes::slotTrackLoaded(TrackPointer pTrack) {
    emit trackLoaded(this, pTrack);
}

void DeckAttributes::slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack) {
    //qDebug() << "DeckAttributes::slotLoadingTrack";
    emit loadingTrack(this, pNewTrack, pOldTrack);
}

void DeckAttributes::slotPlayerEmpty() {
    emit playerEmpty(this);
}

void DeckAttributes::slotRateChanged(double v) {
    Q_UNUSED(v);
    emit rateChanged(this);
}

TrackPointer DeckAttributes::getLoadedTrack() const {
    return m_pPlayer != nullptr ? m_pPlayer->getLoadedTrack() : TrackPointer();
}

AutoDJProcessor::AutoDJProcessor(
        QObject* pParent,
        UserSettingsPointer pConfig,
        PlayerManagerInterface* pPlayerManager,
        TrackCollectionManager* pTrackCollectionManager,
        int iAutoDJPlaylistId)
        : QObject(pParent),
          m_pConfig(pConfig),
          m_pAutoDJTableModel(nullptr),
          m_eState(ADJ_DISABLED),
          m_transitionProgress(0.0),
          m_transitionTime(kTransitionPreferenceDefault),
          m_tandaGapSeconds(kTandaGapPreferenceDefault),
          m_keepQueueRow(0),
          m_keepQueueReloading(false),
          m_keepQueueUpcomingSeconds(0.0),
          m_keepQueueUpcomingTracks(0),
          m_keepQueueTotalSeconds(0.0),
          m_keepQueueTotalTracks(0),
          m_keepQueueCortinaSeconds(45),
          m_keepQueueUpcomingCortinas(0),
          m_keepQueueTotalCortinas(0),
          m_keepQueueDurationDirty(true),
          m_cortinaFadeEnabled(false),
          m_cortinaFadeInSeconds(5),
          m_cortinaFadeOutSeconds(5),
          m_cortinaGapSeconds(kTandaGapPreferenceDefault),
          m_cortinaFadePhase(CortinaFadePhase::None),
          m_cortinaEnvelopeStartSecond(kKeepPosition),
          m_cortinaManualFadeOutStartSecond(kKeepPosition),
          m_cortinaManualFadeOutStartGain(1.0),
          m_pCortinaDeck(nullptr),
          m_cortinaTrackId(),
          m_pTandaFromDeck(nullptr),
          m_pTandaToDeck(nullptr),
          m_tandaToTrackId(),
          m_tandaEntrySecond(0.0),
          m_tandaGapCrossfaderStart(0.0),
          m_tandaGapCrossfaderTarget(0.0),
          m_tandaGapCrossfaderDurationMs(0),
          m_pPlayerManager(pPlayerManager),
          m_coCrossfader(QStringLiteral("[Master]"), QStringLiteral("crossfader")),
          m_coCrossfaderReverse(QStringLiteral("[Mixer Profile]"), QStringLiteral("xFaderReverse")),
          m_shufflePlaylist(ConfigKey(kControlGroup, QStringLiteral("shuffle_playlist"))),
          m_skipNext(ConfigKey(kControlGroup, QStringLiteral("skip_next"))),
          m_addRandomTrack(ConfigKey(kControlGroup, QStringLiteral("add_random_track"))),
          m_fadeNow(ConfigKey(kControlGroup, QStringLiteral("fade_now"))),
          m_enabledAutoDJ(ConfigKey(kControlGroup, QStringLiteral("enabled"))),
          m_keepQueue(ConfigKey(kControlGroup, QStringLiteral("keep_queue"))),
          m_keepQueueOff(ConfigKey(kControlGroup, QStringLiteral("keep_queue_off"))),
          m_pauseAfterDeck(ConfigKey(kControlGroup, QStringLiteral("pause_after_deck"))),
          m_cortinaLength(ConfigKey(kControlGroup, QStringLiteral("cortina_length"))),
          m_showAdjSetTime(ConfigKey(QStringLiteral("[TangoQ]"),
                                   QStringLiteral("show_adj_set_time")),
                  true,
                  1.0),
          m_showAdjEndTime(ConfigKey(QStringLiteral("[TangoQ]"),
                                   QStringLiteral("show_adj_end_time")),
                  true,
                  1.0),
          m_showAdjNudge(ConfigKey(QStringLiteral("[TangoQ]"),
                                 QStringLiteral("show_adj_nudge")),
                  true,
                  1.0),
          m_showCountdownTimer(ConfigKey(QStringLiteral("[TangoQ]"),
                                       QStringLiteral("show_countdown_timer")),
                  true,
                  1.0),
          m_showProgressPips(ConfigKey(QStringLiteral("[TangoQ]"),
                                     QStringLiteral("show_progress_pips")),
                  true,
                  1.0),
          m_resetQueueState(ConfigKey(kControlGroup, QStringLiteral("reset_queue_state"))),
          m_liveMode(ConfigKey(kControlGroup, QStringLiteral("live_mode"))),
          m_hudCountdownSeconds(
                  ConfigKey(kControlGroup, QStringLiteral("hud_countdown_seconds"))),
          m_hudNextKind(
                  ConfigKey(kControlGroup, QStringLiteral("hud_next_kind"))),
          m_hudTandaTrackCount(
                  ConfigKey(kControlGroup, QStringLiteral("hud_tanda_track_count"))),
          m_hudTandaPlayingIndex(
                  ConfigKey(kControlGroup, QStringLiteral("hud_tanda_playing_index"))),
          m_stopGuardArmed(false),
          m_bStopWhenLastTrackEnds(false),
          m_bPauseAfterPending(false) {
    m_pAutoDJTableModel = make_parented<PlaylistTableModel>(
            this, pTrackCollectionManager, "mixxx.db.model.autodj");
    m_pAutoDJTableModel->selectPlaylist(iAutoDJPlaylistId);
    m_pAutoDJTableModel->select();

    // In Keep Queue mode the cursor must stay anchored to the playing track when
    // the user edits the queue. The model rebuilds itself on every edit (emitting
    // row insert/remove), so re-anchor the cursor after each rebuild.
    // Disabling Auto DJ disconnects every deck signal from this object, so the
    // deck warning cannot be driven from playerTrackLoaded(): marks are set with
    // Auto DJ stopped, and the track is often put on a deck by hand afterwards.
    // PlayerInfo reports track changes regardless of Auto DJ's own connections.
    // Queued deliberately. PlayerInfo emits this from inside
    // BaseTrackPlayerImpl::unloadTrack(), which the deck's own destructor calls
    // on shutdown - so answering inline would walk m_decks while PlayerManager
    // is part-way through deleting the decks, reading a deck that is either
    // half-destroyed (its vtable demoted to the abstract base) or already freed.
    // Deferring to the event loop means the work happens after the load or eject
    // has settled, and on shutdown the loop is already gone so it is simply
    // dropped - which is right, since the skin it updates is deleted first.
    connect(&PlayerInfo::instance(),
            &PlayerInfo::trackChanged,
            this,
            [this](const QString&, TrackPointer, TrackPointer) {
                updatePauseAfterDeckControl();
            },
            Qt::QueuedConnection);
    // A mark can be set or cleared while its track is already on a deck, so the
    // deck's warning has to follow it.
    connect(m_pAutoDJTableModel.get(),
            &BaseTrackTableModel::pauseAfterRowsChanged,
            this,
            &AutoDJProcessor::updatePauseAfterDeckControl);
    connect(m_pAutoDJTableModel.get(),
            &QAbstractItemModel::rowsInserted,
            this,
            &AutoDJProcessor::reanchorKeepQueueCursor);
    connect(m_pAutoDJTableModel.get(),
            &QAbstractItemModel::rowsRemoved,
            this,
            &AutoDJProcessor::reanchorKeepQueueCursor);

    // Any queue edit changes the upcoming-tracks total, so invalidate the cached
    // set duration. Recompute is lazy (in getRemainingSetDuration), so the order
    // relative to reanchorKeepQueueCursor above does not matter.
    connect(m_pAutoDJTableModel.get(),
            &QAbstractItemModel::rowsInserted,
            this,
            &AutoDJProcessor::invalidateRemainingSetDuration);
    connect(m_pAutoDJTableModel.get(),
            &QAbstractItemModel::rowsRemoved,
            this,
            &AutoDJProcessor::invalidateRemainingSetDuration);
    connect(m_pAutoDJTableModel.get(),
            &QAbstractItemModel::dataChanged,
            this,
            &AutoDJProcessor::invalidateRemainingSetDuration);
    connect(m_pAutoDJTableModel.get(),
            &QAbstractItemModel::modelReset,
            this,
            &AutoDJProcessor::invalidateRemainingSetDuration);
    connect(m_pAutoDJTableModel.get(),
            &QAbstractItemModel::layoutChanged,
            this,
            &AutoDJProcessor::invalidateRemainingSetDuration);

    // Marking a track as a cortina changes its budgeted play time from the full
    // duration to the cortina length, so the cached totals go stale. The registry
    // is separate from the queue, so none of the model signals above fire for it
    // and the Set Length would otherwise keep showing the pre-marking figure.
    connect(&CortinaRegistry::instance(),
            &CortinaRegistry::cortinaMarksChanged,
            this,
            &AutoDJProcessor::invalidateRemainingSetDuration);

    connect(&m_shufflePlaylist,
            &ControlPushButton::valueChanged,
            this,
            &AutoDJProcessor::controlShuffle);
    connect(&m_skipNext, &ControlObject::valueChanged, this, &AutoDJProcessor::controlSkipNext);
    connect(&m_addRandomTrack,
            &ControlObject::valueChanged,
            this,
            &AutoDJProcessor::controlAddRandomTrack);
    connect(&m_fadeNow, &ControlObject::valueChanged, this, &AutoDJProcessor::controlFadeNow);
    m_enabledAutoDJ.setButtonMode(ControlPushButton::TOGGLE);
    m_enabledAutoDJ.connectValueChangeRequest(this,
            &AutoDJProcessor::controlEnableChangeRequest);

    // Initialize the Tango DJ mode control from the persistent setting and keep
    // the setting in sync when the control changes (e.g. from the prefs dialog).
    m_keepQueue.set(m_pConfig->getValue<bool>(
                            ConfigKey(kPreferenceGroup, QStringLiteral("KeepQueue")))
                    ? 1.0
                    : 0.0);
    m_keepQueueOff.set(m_keepQueue.toBool() ? 0.0 : 1.0);
    connect(&m_keepQueue,
            &ControlObject::valueChanged,
            this,
            &AutoDJProcessor::controlKeepQueue);
    // Toggle on the rising edge so a keyboard shortcut or controller button
    // flips Tango mode, and vet every request centrally: the preferences
    // checkbox disables itself while Auto DJ runs, but nothing stopped the other
    // routes from switching mode mid-set. Connected after the initial set above,
    // which must go through unconditionally to restore the persisted setting.
    m_keepQueue.setButtonMode(ControlPushButton::TOGGLE);
    m_keepQueue.connectValueChangeRequest(this,
            &AutoDJProcessor::controlKeepQueueChangeRequest);

    // The cockpit visibility toggles are driven by 2-state skin buttons, so the
    // controls must be TOGGLE. In the default PUSH mode a button press is
    // momentary - it sets 1 on press and snaps back to 0 on release, so the group
    // only flashes into view and never stays. Skin-created controls get TOGGLE
    // from the parser; these are created here, so set it explicitly.
    m_showAdjSetTime.setButtonMode(ControlPushButton::TOGGLE);
    m_showAdjEndTime.setButtonMode(ControlPushButton::TOGGLE);
    m_showAdjNudge.setButtonMode(ControlPushButton::TOGGLE);
    m_showCountdownTimer.setButtonMode(ControlPushButton::TOGGLE);
    m_showProgressPips.setButtonMode(ControlPushButton::TOGGLE);

    // Live cortina length: initialize from the persistent default and keep the
    // config (and the envelope budget) in sync when it is nudged from the cockpit
    // or applied from Preferences.
    {
        const int cortinaLength = m_pConfig->getValue(
                ConfigKey(kPreferenceGroup, QStringLiteral("CortinaLength")), 45);
        m_cortinaLength.set(cortinaLength);
        m_keepQueueCortinaSeconds = cortinaLength;
    }
    connect(&m_cortinaLength,
            &ControlObject::valueChanged,
            this,
            &AutoDJProcessor::controlCortinaLength);

    connect(&m_resetQueueState,
            &ControlObject::valueChanged,
            this,
            &AutoDJProcessor::controlResetQueueState);

    // LIVE mode is session-only (not restored from config), so it always starts
    // off. The stop guard auto-disarms a few seconds after the first disable
    // request if no confirming second request arrives.
    m_stopGuardTimer.setSingleShot(true);
    m_stopGuardTimer.setInterval(3000);
    connect(&m_stopGuardTimer,
            &QTimer::timeout,
            this,
            &AutoDJProcessor::disarmStopGuard);

    // One-shot timer that holds the Nc silent gaps before and after a cortina.
    m_cortinaGapTimer.setSingleShot(true);
    connect(&m_cortinaGapTimer,
            &QTimer::timeout,
            this,
            &AutoDJProcessor::slotCortinaGapElapsed);

    // One-shot timer that holds the real silent gap for Tanda Transition.
    m_tandaGapTimer.setSingleShot(true);
    connect(&m_tandaGapTimer,
            &QTimer::timeout,
            this,
            &AutoDJProcessor::slotTandaGapElapsed);
    m_tandaGapCrossfaderTimer.setSingleShot(false);
    m_tandaGapCrossfaderTimer.setInterval(50);
    connect(&m_tandaGapCrossfaderTimer,
            &QTimer::timeout,
            this,
            &AutoDJProcessor::slotTandaGapProgress);

    // 1 Hz publisher for the Tango HUD countdown. Cheap (no DB reads) and runs on
    // the GUI thread, so it cannot affect audio. publishHudTiming() early-returns
    // unless Auto DJ is running in Tango mode.
    m_hudCountdownSeconds.set(-1.0);
    m_hudNextKind.set(0.0);
    m_hudTandaTrackCount.set(0.0);
    m_hudTandaPlayingIndex.set(-1.0);
    m_hudTimer.setInterval(1000);
    connect(&m_hudTimer,
            &QTimer::timeout,
            this,
            &AutoDJProcessor::publishHudTiming);
    m_hudTimer.start();

    connect(pPlayerManager,
            &PlayerManagerInterface::numberOfDecksChanged,
            this,
            &AutoDJProcessor::slotNumberOfDecksChanged);
    slotNumberOfDecksChanged(pPlayerManager->numberOfDecks());

    QString str_autoDjTransition = m_pConfig->getValueString(
            ConfigKey(kPreferenceGroup, kTransitionPreferenceName));
    if (!str_autoDjTransition.isEmpty()) {
        m_transitionTime = str_autoDjTransition.toDouble();
    }

    m_transitionMode = m_pConfig->getValue(
            ConfigKey(kPreferenceGroup, kTransitionModePreferenceName),
            TransitionMode::FullIntroOutro);
    if (!keepQueueEnabled() && isTandaTransition(m_transitionMode)) {
        restoreLastStockTransitionMode();
    } else if (isStockTransitionMode(m_transitionMode)) {
        rememberStockTransitionMode(m_transitionMode);
    }

    m_tandaGapSeconds = m_pConfig->getValue(
            ConfigKey(kPreferenceGroup, kTandaGapPreferenceName),
            kTandaGapPreferenceDefault);

    loadCortinaFadeSettings();
}

void AutoDJProcessor::slotNumberOfDecksChanged(int decks) {
    m_decks.reserve(decks);
    // Add more decks if we have not all yet.
    // Mixxx does not support reducing the number of deck
    for (int i = static_cast<int>(m_decks.size()); i < decks; ++i) {
        BaseTrackPlayer* pPlayer = m_pPlayerManager->getDeckBase(i);
        // Shouldn't be possible.
        VERIFY_OR_DEBUG_ASSERT(pPlayer) {
            return;
        }
        m_decks.emplace_back(std::make_unique<DeckAttributes>(i, pPlayer));
    }
}

double AutoDJProcessor::getCrossfader() const {
    if (m_coCrossfaderReverse.toBool()) {
        return m_coCrossfader.get() * -1.0;
    }
    return m_coCrossfader.get();
}

void AutoDJProcessor::setCrossfader(double value) {
    if (m_coCrossfaderReverse.toBool()) {
        value *= -1.0;
    }
    m_coCrossfader.set(value);
}

void AutoDJProcessor::setAutoDJFadeGain(DeckAttributes* pDeck, double gain) {
    if (!pDeck) {
        return;
    }
    pDeck->setAutoDJFadeGain(std::clamp(gain, 0.0, 1.0));
}

void AutoDJProcessor::resetAllAutoDJFadeGains() {
    for (const auto& pDeck : m_decks) {
        if (pDeck) {
            pDeck->resetAutoDJFadeGain();
        }
    }
}

void AutoDJProcessor::startTandaCrossfaderAnimation(
        DeckAttributes* pFromDeck,
        DeckAttributes* pToDeck,
        int gapMs) {
    Q_UNUSED(pFromDeck);
    stopTandaCrossfaderAnimation();
    if (!pToDeck) {
        return;
    }
    m_tandaGapCrossfaderStart = getCrossfader();
    m_tandaGapCrossfaderTarget = pToDeck->isLeft() ? -1.0 : 1.0;
    m_tandaGapCrossfaderDurationMs = std::max(0, gapMs);
    if (m_tandaGapCrossfaderDurationMs == 0) {
        setCrossfader(m_tandaGapCrossfaderTarget);
        return;
    }
    m_tandaGapCrossfaderClock.restart();
    m_tandaGapCrossfaderTimer.start();
    updateTandaCrossfaderAnimation();
}

void AutoDJProcessor::updateTandaCrossfaderAnimation() {
    if (m_tandaGapCrossfaderDurationMs <= 0) {
        setCrossfader(m_tandaGapCrossfaderTarget);
        stopTandaCrossfaderAnimation();
        return;
    }
    const double progress = std::clamp(
            static_cast<double>(m_tandaGapCrossfaderClock.elapsed()) /
                    static_cast<double>(m_tandaGapCrossfaderDurationMs),
            0.0,
            1.0);
    setCrossfader(m_tandaGapCrossfaderStart +
            (m_tandaGapCrossfaderTarget - m_tandaGapCrossfaderStart) * progress);
    if (progress >= 1.0) {
        stopTandaCrossfaderAnimation();
    }
}

void AutoDJProcessor::stopTandaCrossfaderAnimation() {
    m_tandaGapCrossfaderTimer.stop();
    m_tandaGapCrossfaderDurationMs = 0;
}

void AutoDJProcessor::slotTandaGapProgress() {
    updateTandaCrossfaderAnimation();
}

AutoDJProcessor::AutoDJError AutoDJProcessor::shufflePlaylist(
        const QModelIndexList& selectedIndices) {
    if (keepQueueEnabled()) {
        // Shuffling would destroy the pre-arranged order in Keep Queue mode.
        return ADJ_OK;
    }
    QModelIndex exclude;
    if (m_eState != ADJ_DISABLED) {
        exclude = m_pAutoDJTableModel->index(0, 0);
    }
    m_pAutoDJTableModel->shuffleTracks(selectedIndices, exclude);
    return ADJ_OK;
}

void AutoDJProcessor::fadeNow() {
    if (m_eState != ADJ_IDLE) {
        // we cannot fade if AutoDj is disabled or already fading
        return;
    }
    if (keepQueueEnabled()) {
        // In Tango mode the next track is reached deliberately with the
        // crossfader, so Fade Now is disabled.
        return;
    }

    double crossfader = getCrossfader();
    DeckAttributes* pLeftDeck = getLeftDeck();
    DeckAttributes* pRightDeck = getRightDeck();
    if (!pLeftDeck || !pRightDeck) {
        // User has changed the orientation, disable Auto DJ
        toggleAutoDJ(false);
        emit autoDJError(ADJ_NOT_TWO_DECKS);
        return;
    }

    DeckAttributes* pFromDeck;
    DeckAttributes* pToDeck;

    if (pLeftDeck->isPlaying() &&
            (!pRightDeck->isPlaying() || crossfader < 0.0)) {
        pFromDeck = pLeftDeck;
        pToDeck = pRightDeck;
    } else if (pRightDeck->isPlaying()) {
        pFromDeck = pRightDeck;
        pToDeck = pLeftDeck;
    } else {
        // Neither deck is playing. Fading now makes no sense.
        return;
    }

    pFromDeck->setRepeat(false);
    pFromDeck->isFromDeck = true;
    pToDeck->isFromDeck = false;

    const double fromDeckEndSecond = getEndSecond(pFromDeck);
    const double toDeckEndSecond = getEndSecond(pToDeck);
    // Since the end position is measured in seconds from 0:00 it is also
    // the track duration. Use this alias for better readability.
    const double fromDeckDuration = fromDeckEndSecond;
    const double toDeckDuration = toDeckEndSecond;
    if (toDeckDuration < kMinimumTrackDurationSec) {
        // Deck is empty or track too short, disable AutoDJ
        // This happens only if the user has changed deck orientation to such deck.
        toggleAutoDJ(false);
        emit autoDJError(ADJ_NOT_TWO_DECKS);
        return;
    }

    // playPosition() is in the range of 0..1
    const double fromDeckCurrentSecond = fromDeckDuration * pFromDeck->playPosition();
    const double toDeckCurrentSecond = toDeckDuration * pToDeck->playPosition();

    if (toDeckDuration - toDeckCurrentSecond < kMinimumTrackDurationSec) {
        // Remaining Track time is too short, user has has seeked near the end
        // Re-cue the track
        pToDeck->setPlayPosition(pToDeck->startPos);
    }

    pFromDeck->fadeBeginPos = fromDeckCurrentSecond;
    // Do not seek to a calculated start point; start the to deck from wherever
    // it is if the user has seeked since loading the track.
    pToDeck->startPos = toDeckCurrentSecond;

    // If the user presses "Fade now", assume they want to fade *now*, not later.
    // So if the spinbox time is negative, do not insert silence.
    double spinboxTime = fabs(m_transitionTime);

    double fadeTime;
    if (m_transitionMode == TransitionMode::FullIntroOutro ||
            m_transitionMode == TransitionMode::FadeAtOutroStart) {
        // Use the intro length as the transition time. If the user has seeked
        // away from the intro start since the track was loaded, start from
        // there and do not seek back to the intro start. If they have seeked
        // past the introEnd or the introEnd is not marked, fall back to the
        // spinbox time.
        double outroEnd = getOutroEndSecond(pFromDeck);
        double introEnd = getIntroEndSecond(pToDeck);
        double introStart = getIntroStartSecond(pToDeck);
        double timeUntilOutroEnd = outroEnd - fromDeckCurrentSecond;

        // IntroStart ends up being equal to introEnd when pToDeck is
        // paused and its introEnd marker is not set. getIntroEndSecond returns
        // introStart thus the two end up having equal values
        if (toDeckCurrentSecond >= introStart &&
                toDeckCurrentSecond <= introEnd &&
                introStart != introEnd) {
            double timeUntilIntroEnd = introEnd - toDeckCurrentSecond;
            // The fade must end by the outro end at the latest.
            fadeTime = math_min(timeUntilIntroEnd, timeUntilOutroEnd);
        } else {
            // If this is true, the fade should have already been started
            // so the user should not have been able to press the Fade button.
            VERIFY_OR_DEBUG_ASSERT(timeUntilOutroEnd > 0) {
                timeUntilOutroEnd = 0;
            }
            fadeTime = math_min(spinboxTime, timeUntilOutroEnd);
        }
    } else {
        fadeTime = spinboxTime;
    }

    fadeTime = math_min(fadeTime, fromDeckEndSecond - fromDeckCurrentSecond);
    fadeTime = math_min(fadeTime,
            (toDeckEndSecond - toDeckCurrentSecond) / 2); // for fade in and out

    pFromDeck->fadeEndPos = fromDeckCurrentSecond + fadeTime;

    // These are expected to be a fraction of the track length.
    pFromDeck->fadeBeginPos /= fromDeckDuration;
    pFromDeck->fadeEndPos /= fromDeckDuration;
    pToDeck->startPos /= toDeckDuration;

    VERIFY_OR_DEBUG_ASSERT(pFromDeck->fadeBeginPos <= 1) {
        pFromDeck->fadeBeginPos = 1;
    }
}

bool AutoDJProcessor::canFadePlayingCortinaNow() const {
    return keepQueueEnabled() &&
            m_eState != ADJ_DISABLED &&
            m_cortinaFadePhase != CortinaFadePhase::AfterGap &&
            m_cortinaManualFadeOutStartSecond == kKeepPosition &&
            playingCortinaDeck();
}

bool AutoDJProcessor::fadePlayingCortinaNow() {
    DeckAttributes* pCortinaDeck = playingCortinaDeck();
    qInfo().nospace() << "[CORTINA_FADE_NOW] entry keepQueue="
                      << yesNo(keepQueueEnabled())
                      << " state=" << static_cast<int>(m_eState)
                      << " phase=" << static_cast<int>(m_cortinaFadePhase)
                      << " fadeMode=" << yesNo(m_cortinaFadeEnabled)
                      << " transitionMode=" << transitionModeName(m_transitionMode)
                      << " playingCortinaDeck="
                      << (pCortinaDeck ? pCortinaDeck->group : QStringLiteral("<none>"));
    if (!pCortinaDeck) {
        for (const auto& pDeck : m_decks) {
            if (!pDeck) {
                continue;
            }
            const TrackPointer pTrack = pDeck->getLoadedTrack();
            const TrackId trackId = pTrack ? TrackId(pTrack->getId()) : TrackId();
            qInfo().nospace() << "[CORTINA_FADE_NOW] deck group=" << pDeck->group
                              << " playing=" << yesNo(pDeck->isPlaying())
                              << " loading=" << yesNo(pDeck->loading)
                              << " trackId=" << trackId.toString()
                              << " isCortina=" << yesNo(isCortina(pTrack))
                              << " playpos=" << pDeck->playPosition()
                              << " fadeGain=" << pDeck->autoDJFadeGain();
        }
    }
    if (!canFadePlayingCortinaNow() || !pCortinaDeck) {
        qInfo().nospace() << "[CORTINA_FADE_NOW] reject reason=canFadeFalse";
        publishHudTiming();
        return false;
    }
    const TrackPointer pCortina = pCortinaDeck->getLoadedTrack();
    const double duration = getEndSecond(pCortinaDeck);
    if (!pCortina || duration < kMinimumTrackDurationSec) {
        qInfo().nospace() << "[CORTINA_FADE_NOW] reject reason=trackOrDuration"
                          << " hasTrack=" << yesNo(static_cast<bool>(pCortina))
                          << " duration=" << duration;
        publishHudTiming();
        return false;
    }

    const double currentSecond = duration * pCortinaDeck->playPosition();
    const double firstSound = getFirstSoundSecond(pCortinaDeck);
    const double currentGain = std::clamp(
            pCortinaDeck->autoDJFadeGain(),
            0.0,
            1.0);
    const double targetZ = math_min(
            static_cast<double>(m_cortinaFadeOutSeconds),
            math_max(getLastSoundSecond(pCortinaDeck) - currentSecond, 0.0));
    qInfo().nospace() << "[CORTINA_FADE_NOW] arm deck=" << pCortinaDeck->group
                      << " trackId=" << TrackId(pCortina->getId()).toString()
                      << " duration=" << duration
                      << " currentSecond=" << currentSecond
                      << " firstSound=" << firstSound
                      << " currentGain=" << currentGain
                      << " targetZ=" << targetZ
                      << " deckFadeGainBefore="
                      << pCortinaDeck->autoDJFadeGain()
                      << " crossfaderBefore=" << getCrossfader();
    m_pCortinaDeck = pCortinaDeck;
    m_cortinaTrackId = TrackId(pCortina->getId());
    m_cortinaFadePhase = CortinaFadePhase::Envelope;
    // Manual fades start from the deck's actual current Auto DJ gain, not from a
    // synthetic position in the automatic X/Y/Z envelope.
    m_cortinaManualFadeOutStartSecond = currentSecond;
    m_cortinaManualFadeOutStartGain = currentGain;
    setCrossfader(pCortinaDeck->isLeft() ? -1.0 : 1.0);
    const bool handled = maybeHandleCortinaFade(pCortinaDeck, pCortinaDeck->playPosition());
    qInfo().nospace() << "[CORTINA_FADE_NOW] handled=" << yesNo(handled)
                      << " phase=" << static_cast<int>(m_cortinaFadePhase)
                      << " envelopeStart=" << m_cortinaEnvelopeStartSecond
                      << " deckFadeGainAfter="
                      << pCortinaDeck->autoDJFadeGain()
                      << " crossfaderAfter=" << getCrossfader()
                      << " deckPlaying=" << yesNo(pCortinaDeck->isPlaying());
    publishHudTiming();
    return handled;
}

AutoDJProcessor::AutoDJError AutoDJProcessor::skipNext() {
    if (m_eState == ADJ_DISABLED) {
        emit autoDJError(ADJ_IS_INACTIVE);
        return ADJ_IS_INACTIVE;
    }
    if (keepQueueEnabled()) {
        // Skip is disabled in Keep Queue mode (may be armed later).
        return ADJ_OK;
    }
    // Load the next song from the queue.
    DeckAttributes* pLeftDeck = getLeftDeck();
    DeckAttributes* pRightDeck = getRightDeck();
    if (!pLeftDeck || !pRightDeck) {
        // User has changed the orientation, disable Auto DJ
        toggleAutoDJ(false);
        emit autoDJError(ADJ_NOT_TWO_DECKS);
        return ADJ_NOT_TWO_DECKS;
    }

    if (!pLeftDeck->isPlaying()) {
        removeLoadedTrackFromTopOfQueue(*pLeftDeck);
        loadNextTrackFromQueue(*pLeftDeck);
    } else if (!pRightDeck->isPlaying()) {
        removeLoadedTrackFromTopOfQueue(*pRightDeck);
        loadNextTrackFromQueue(*pRightDeck);
    } else {
        // If both decks are playing remove next track in playlist
        TrackId nextId = m_pAutoDJTableModel->getTrackId(m_pAutoDJTableModel->index(0, 0));
        TrackId leftId = pLeftDeck->getLoadedTrack()->getId();
        TrackId rightId = pRightDeck->getLoadedTrack()->getId();
        if (nextId == leftId || nextId == rightId) {
        // One of the playing tracks is still on top of playlist, remove second item
            m_pAutoDJTableModel->removeTrack(m_pAutoDJTableModel->index(1, 0));
        } else {
            m_pAutoDJTableModel->removeTrack(m_pAutoDJTableModel->index(0, 0));
        }
        maybeFillRandomTracks();
    }
    return ADJ_OK;
}

AutoDJProcessor::AutoDJError AutoDJProcessor::toggleAutoDJ(bool enable) {
    // LIVE mode accidental-stop guard (Tango mode only): while a set is running,
    // the first disable request only arms a short confirmation window; a second
    // request within it actually stops. Covers the button, Shift+F12 and MIDI.
    // The automatic end-of-set stop is exempt: the queue really has run out, so
    // there is no accident to guard against, and holding it behind a
    // confirmation the DJ never gives would leave Auto DJ on after the set.
    if (!enable && !automaticStopPending() && liveModeEnabled() &&
            keepQueueEnabled() && m_eState != ADJ_DISABLED) {
        if (!m_stopGuardArmed) {
            m_stopGuardArmed = true;
            m_stopGuardTimer.start();
            // Keep Auto DJ enabled; the toolbar shows a "Confirm Stop?" prompt.
            m_enabledAutoDJ.setAndConfirm(1.0);
            emit stopGuardArmedChanged(true);
            return ADJ_OK;
        }
        // Second request within the window: confirmed stop, fall through.
        disarmStopGuard();
    } else if (m_stopGuardArmed) {
        // Any other request (e.g. re-enable) cancels a pending arm.
        disarmStopGuard();
    }

    const bool pauseAfterStopPending = m_bPauseAfterPending;

    // Past the guard the toggle is going through, so the pending end-of-queue
    // stop has either just been carried out or been superseded by a deliberate
    // one. Either way it is spent.
    m_bStopWhenLastTrackEnds = false;
    m_bPauseAfterPending = false;
    if (enable || !pauseAfterStopPending) {
        m_pAutoDJTableModel->clearActivePauseAfterRow();
    }

    if (enable) { // Enable Auto DJ
        DeckAttributes* pLeftDeck = getLeftDeck();
        DeckAttributes* pRightDeck = getRightDeck();
        if (!pLeftDeck || !pRightDeck) {
            // Keep the current state.
            emitAutoDJStateChanged(m_eState);
            emit autoDJError(ADJ_NOT_TWO_DECKS);
            return ADJ_NOT_TWO_DECKS;
        }

        resetAllAutoDJFadeGains();
        stopTandaCrossfaderAnimation();

        bool leftDeckPlaying = pLeftDeck->isPlaying();
        bool rightDeckPlaying = pRightDeck->isPlaying();

        if (leftDeckPlaying && rightDeckPlaying) {
            qDebug() << "One deck must be stopped before enabling Auto DJ mode";
            // Keep the current state.
            emitAutoDJStateChanged(m_eState);
            emit autoDJError(ADJ_BOTH_DECKS_PLAYING);
            return ADJ_BOTH_DECKS_PLAYING;
        }
        // Auto-DJ needs at least two decks
        DEBUG_ASSERT(m_decks.size() > 1);

        // TODO: This is a total band aid for making Auto DJ work with four decks.
        // We should design a nicer way to handle this.
        for (const auto& pDeck : m_decks) {
            VERIFY_OR_DEBUG_ASSERT(pDeck) {
                continue;
            }
            if (pDeck.get() == pLeftDeck) {
                continue;
            }
            if (pDeck.get() == pRightDeck) {
                continue;
            }
            if (pDeck->isPlaying()) {
                // Keep the current state.
                emitAutoDJStateChanged(m_eState);
                emit autoDJError(ADJ_UNUSED_DECK_PLAYING);
                return ADJ_UNUSED_DECK_PLAYING;
            }
        }

        if (pLeftDeck->index > 1 || pRightDeck->index > 1) {
            // Left and/or right deck is deck 3/4 which may not be visible.
            // Make sure it is, if the current skin is a 4-deck skin.
            ControlObject::set(
                    ConfigKey(QStringLiteral("[Skin]"), QStringLiteral("show_4decks")), 1);
        }

        // Snapshot the Cortina Fade settings for this set. They are only
        // editable while Auto DJ is stopped, and reading them here keeps the
        // engine behaviour independent of the UI refresh timer. TandaGap stays
        // live through setTandaGapSeconds(), but read config here too so a
        // value changed before the processor existed is picked up.
        m_tandaGapSeconds = m_pConfig->getValue(
                ConfigKey(kPreferenceGroup, kTandaGapPreferenceName),
                kTandaGapPreferenceDefault);
        loadCortinaFadeSettings();

        // Keep Queue mode: always continue from the next unplayed track (the
        // cursor). Only keep it in bounds in case the queue shrank while Auto DJ
        // was stopped. (Clearing the queue restarts from the top; that is handled
        // by the empty-queue reset in reanchorKeepQueueCursor().)
        if (keepQueueEnabled() && m_keepQueueRow > m_pAutoDJTableModel->rowCount()) {
            m_keepQueueRow = m_pAutoDJTableModel->rowCount();
        }

        // Whether the deck that ends up playing took its track off the queue -
        // i.e. the cursor moved past it here. It decides how an exhausted queue
        // is read below: "the last queued track is now playing" and "you have
        // nothing queued" look identical afterwards but want opposite answers.
        bool playingFromQueue = false;

        // Never load the same track if it is already playing
        if (leftDeckPlaying) {
            playingFromQueue = removeLoadedTrackFromTopOfQueue(*pLeftDeck);
        } else if (rightDeckPlaying) {
            playingFromQueue = removeLoadedTrackFromTopOfQueue(*pRightDeck);
        } else {
            // If the first track is already cued at a position in the first
            // 2/3 in on of the Auto DJ decks, start it.
            // If the track is paused at a later position, it is probably too
            // close to the end. In this case it is loaded again at the stored
            // cue point.
            if (pLeftDeck->playPosition() < 0.66 &&
                    removeLoadedTrackFromTopOfQueue(*pLeftDeck)) {
                pLeftDeck->play();
                leftDeckPlaying = true;
                playingFromQueue = true;
            } else if (pRightDeck->playPosition() < 0.66 &&
                    removeLoadedTrackFromTopOfQueue(*pRightDeck)) {
                pRightDeck->play();
                rightDeckPlaying = true;
                playingFromQueue = true;
            }
        }

        TrackPointer nextTrack = getNextTrackFromQueue();
        if (!nextTrack && !playingFromQueue) {
            // Nothing left to play and nothing of ours playing, so there is
            // genuinely nothing to do. A deck the DJ started by hand does not
            // count: refusing there is what tells them the queue is empty, and
            // going ahead would eject the other deck under them.
            qDebug() << "Queue is empty now, disable Auto DJ";
            m_enabledAutoDJ.setAndConfirm(0.0);
            emitAutoDJStateChanged(m_eState);
            emit autoDJError(ADJ_QUEUE_EMPTY);
            return ADJ_QUEUE_EMPTY;
        }

        // Track is available so GO
        m_enabledAutoDJ.setAndConfirm(1.0);
        qDebug() << "Auto DJ enabled";

        m_coCrossfader.connectValueChanged(this, &AutoDJProcessor::crossfaderChanged);

        connect(pLeftDeck,
                &DeckAttributes::playPositionChanged,
                this,
                &AutoDJProcessor::playerPositionChanged);
        connect(pRightDeck,
                &DeckAttributes::playPositionChanged,
                this,
                &AutoDJProcessor::playerPositionChanged);

        connect(pLeftDeck,
                &DeckAttributes::playChanged,
                this,
                &AutoDJProcessor::playerPlayChanged);
        connect(pRightDeck,
                &DeckAttributes::playChanged,
                this,
                &AutoDJProcessor::playerPlayChanged);

        connect(pLeftDeck,
                &DeckAttributes::introStartPositionChanged,
                this,
                &AutoDJProcessor::playerIntroStartChanged);
        connect(pRightDeck,
                &DeckAttributes::introStartPositionChanged,
                this,
                &AutoDJProcessor::playerIntroStartChanged);

        connect(pLeftDeck,
                &DeckAttributes::introEndPositionChanged,
                this,
                &AutoDJProcessor::playerIntroEndChanged);
        connect(pRightDeck,
                &DeckAttributes::introEndPositionChanged,
                this,
                &AutoDJProcessor::playerIntroEndChanged);

        connect(pLeftDeck,
                &DeckAttributes::outroStartPositionChanged,
                this,
                &AutoDJProcessor::playerOutroStartChanged);
        connect(pRightDeck,
                &DeckAttributes::outroStartPositionChanged,
                this,
                &AutoDJProcessor::playerOutroStartChanged);

        connect(pLeftDeck,
                &DeckAttributes::outroEndPositionChanged,
                this,
                &AutoDJProcessor::playerOutroEndChanged);
        connect(pRightDeck,
                &DeckAttributes::outroEndPositionChanged,
                this,
                &AutoDJProcessor::playerOutroEndChanged);

        connect(pLeftDeck,
                &DeckAttributes::trackLoaded,
                this,
                &AutoDJProcessor::playerTrackLoaded);
        connect(pRightDeck,
                &DeckAttributes::trackLoaded,
                this,
                &AutoDJProcessor::playerTrackLoaded);

        connect(pLeftDeck,
                &DeckAttributes::loadingTrack,
                this,
                &AutoDJProcessor::playerLoadingTrack);
        connect(pRightDeck,
                &DeckAttributes::loadingTrack,
                this,
                &AutoDJProcessor::playerLoadingTrack);

        connect(pLeftDeck,
                &DeckAttributes::playerEmpty,
                this,
                &AutoDJProcessor::playerEmpty);
        connect(pRightDeck,
                &DeckAttributes::playerEmpty,
                this,
                &AutoDJProcessor::playerEmpty);

        connect(pLeftDeck,
                &DeckAttributes::rateChanged,
                this,
                &AutoDJProcessor::playerRateChanged);
        connect(pRightDeck,
                &DeckAttributes::rateChanged,
                this,
                &AutoDJProcessor::playerRateChanged);
        connect(m_pAutoDJTableModel,
                &PlaylistTableModel::firstTrackChanged,
                this,
                &AutoDJProcessor::playlistFirstTrackChanged);

        if (!leftDeckPlaying && !rightDeckPlaying) {
            // Both decks are stopped. Load a track into deck 1 and start it
            // playing. Instruct playerPositionChanged to wait for a
            // playposition update from deck 1. playerPositionChanged for
            // ADJ_ENABLE_P1LOADED will set the crossfader left and remove the
            // loaded track from the queue and wait for the next call to
            // playerPositionChanged for deck1 after the track is loaded.
            m_eState = ADJ_ENABLE_P1LOADED;

            // Move crossfader to the left.
            setCrossfader(-1.0);

            // Load track into the left deck and play. Once it starts playing,
            // we will receive a playerPositionChanged update for deck 1 which
            // will load a track into the right deck and switch to IDLE mode.
            emitLoadTrackToPlayer(nextTrack, pLeftDeck->group, true);
        } else {
            // One of the two decks is playing. Switch into IDLE mode and wait
            // until the playing deck crosses posThreshold to start fading.
            m_eState = ADJ_IDLE;
            DeckAttributes* pIdleDeck = leftDeckPlaying ? pRightDeck : pLeftDeck;
            if (!nextTrack) {
                // Resumed onto the last track in the queue: there is nothing to
                // cue up behind it. Stay enabled and stop once it ends, exactly
                // as the mid-set dry-queue path does - Auto DJ switching itself
                // off the instant the track starts is the bug this replaces.
                armStopWhenLastTrackEnds();
                // Eject (nextTrack is null) as "End of auto DJ warning".
                emitLoadTrackToPlayer(nextTrack, pIdleDeck->group, false);
            } else if (!keepQueueEnabled() || nextTrack != pIdleDeck->getLoadedTrack()) {
                // In Tango/keep-queue mode the next track stays in the queue and the
                // idle deck keeps its loaded track when Auto DJ is disabled, so a plain
                // toggle off/on would reload the same track onto the idle deck and
                // re-render its (otherwise static) waveform - the flicker the DJ sees on
                // the non-playing deck. Skip the load when it is already cued there.
                emitLoadTrackToPlayer(nextTrack, pIdleDeck->group, false);
            }
            // Move the crossfader away from the idle deck.
            setCrossfader(leftDeckPlaying ? -1.0 : 1.0);
        }
        emitAutoDJStateChanged(m_eState);
    } else { // Disable Auto DJ
        m_enabledAutoDJ.setAndConfirm(0.0);
        qDebug() << "Auto DJ disabled";
        m_eState = ADJ_DISABLED;
        // Cancel any running cortina gap/envelope so a pending gap timer can't
        // start a deck after Auto DJ has been turned off.
        cancelTandaGap();
        cancelCortinaFade();
        resetAllAutoDJFadeGains();
        disconnect(&m_coCrossfader,
                &ControlProxy::valueChanged,
                this,
                &AutoDJProcessor::crossfaderChanged);
        for (const auto& pDeck : m_decks) {
            pDeck->disconnect(this);
        }
        emitAutoDJStateChanged(m_eState);
        publishHudTiming();
    }
    return ADJ_OK;
}

void AutoDJProcessor::controlEnableChangeRequest(double value) {
    toggleAutoDJ(value > 0.0);
}

void AutoDJProcessor::controlFadeNow(double value) {
    if (value > 0.0) {
        fadeNow();
        // Fade Now transitions immediately; refresh the HUD now rather than
        // waiting up to a second for the next timer tick.
        publishHudTiming();
    }
}

void AutoDJProcessor::controlShuffle(double value) {
    if (value > 0.0) {
        shufflePlaylist(QModelIndexList());
    }
}

void AutoDJProcessor::controlSkipNext(double value) {
    if (value > 0.0) {
        skipNext();
    }
}

void AutoDJProcessor::controlAddRandomTrack(double value) {
    if (value > 0.0 && !keepQueueEnabled()) {
        emit randomTrackRequested(1);
    }
}

void AutoDJProcessor::crossfaderChanged(double value) {
    if (isTandaGapPending() || m_tandaGapCrossfaderTimer.isActive() ||
            m_cortinaFadePhase != CortinaFadePhase::None) {
        return;
    }

    if (m_eState == ADJ_IDLE) {
        // The user is changing the crossfader manually. If the user has
        // moved it all the way to the other side, make the deck faded away
        // from the new "to deck" by loading the next track into it.
        DeckAttributes* pFromDeck = getFromDeck();
        VERIFY_OR_DEBUG_ASSERT(pFromDeck) {
            // we have always a from deck in case of state IDLE
            return;
        }

        DeckAttributes* pToDeck = getOtherDeck(pFromDeck);
        if (!pToDeck) {
            // we have always a from deck in case of state IDLE
            // if the user has not changed the deck orientation
            return;
        }

        double crossfaderPosition = value * (m_coCrossfaderReverse.toBool() ? -1 : 1);
        if ((crossfaderPosition == 1.0 && pFromDeck->isLeft()) ||       // crossfader right
                (crossfaderPosition == -1.0 && pFromDeck->isRight())) { // crossfader left
            if (!pToDeck->isPlaying()) {
                if (getEndSecond(pToDeck) >= kMinimumTrackDurationSec) {
                    // Re-cue the track if the user has seeked it to the very end
                    if (pToDeck->playPosition() >= pToDeck->fadeBeginPos) {
                        pToDeck->setPlayPosition(pToDeck->startPos);
                    }
                    pToDeck->play();
                } else {
                    // Track in toDeck was ejected manually, stop.
                    toggleAutoDJ(false);
                    return;
                }
            }
            pFromDeck->stop();

            // Now that we have started the other deck playing, remove the track
            // that was "on deck" from the top of the queue.
            removeLoadedTrackFromTopOfQueue(*pToDeck);
            loadNextTrackFromQueue(*pFromDeck);
        }
    }
}

void AutoDJProcessor::playerPositionChanged(DeckAttributes* pAttributes,
                                            double thisPlayPosition) {
    // qDebug() << "player" << pAttributes->group << "PositionChanged(" << value << ")";
    if (m_eState == ADJ_DISABLED) {
        // nothing to do
        return;
    }

    DeckAttributes* thisDeck = pAttributes;
    DeckAttributes* otherDeck = getOtherDeck(thisDeck);
    if (!otherDeck) {
        // This happens if this deck has no orientation or
        // there is no deck with the opposite orientation
        return;
    }

    // Note: this can be a delayed call of playerPositionChanged() where
    // the track was playing, but is now stopped.
    bool thisDeckPlaying = thisDeck->isPlaying();
    bool otherDeckPlaying = otherDeck->isPlaying();

    // Cortina Fade mode owns the crossfader for a solo-playing cortina (fade in,
    // hold, fade out, then hand off to the next tanda track). When it takes over,
    // skip the normal transition handling for this deck.
    if (maybeHandleCortinaFade(thisDeck, thisPlayPosition)) {
        return;
    }

    // To switch out of ADJ_ENABLE_P1LOADED we wait for a playposition update
    // for either deck.
    if (m_eState == ADJ_ENABLE_P1LOADED) {
        DeckAttributes* leftDeck;
        DeckAttributes* rightDeck;

        if (thisDeck->isLeft()) {
            leftDeck = thisDeck;
            DEBUG_ASSERT(otherDeck->isRight());
            rightDeck = otherDeck;
        } else {
            DEBUG_ASSERT(thisDeck->isRight());
            rightDeck = thisDeck;
            DEBUG_ASSERT(otherDeck->isLeft());
            leftDeck = otherDeck;
        }

        // Note: If a playing deck has reached the end the play state is already reset
        bool leftDeckPlaying = leftDeck->isPlaying();
        bool rightDeckPlaying = rightDeck->isPlaying();
        bool leftDeckReachesEnd = thisDeck->isLeft() && thisPlayPosition >= 1.0;

        if (leftDeckPlaying || rightDeckPlaying || leftDeckReachesEnd) {
            // One of left and right is playing. Switch to IDLE mode and make
            // sure our thresholds are configured (by calling calculateFadeThresholds
            // for the playing deck).
            m_eState = ADJ_IDLE;

            if (!rightDeckPlaying) {
                // Only left deck playing!
                // In ADJ_ENABLE_P1LOADED mode we wait until the left deck
                // successfully starts playing. We don't know in toggleAutoDJ
                // whether the track will load successfully so we have to
                // wait. If the track fails to load then playerTrackLoadFailed
                // will remove it from the top of the queue and request another
                // track. Remove the left deck's current track from the queue
                // since it is the track we requested in toggleAutoDJ.
                removeLoadedTrackFromTopOfQueue(*leftDeck);

                // Load the next track into the right player since it is not
                // playing.
                loadNextTrackFromQueue(*rightDeck);

                // Note: calculateTransition() is called in playerTrackLoaded()
            } else {
                // At least right deck is playing
                // Set crossfade thresholds for right deck.
                if constexpr (sDebug) {
                    qDebug() << this << "playerPositionChanged"
                             << "right deck playing";
                }
                calculateTransition(rightDeck, leftDeck, false);
            }
            emitAutoDJStateChanged(m_eState);
        }
        return;
    }

    // In FADING states, we expect that both tracks are playing.
    // Normally the the fading fromDeck stops after the transition is over and
    // we need to replace it with a new track from the queue.
    if (m_eState == ADJ_LEFT_FADING || m_eState == ADJ_RIGHT_FADING) {
        // Once P1 or P2 has stopped switch out of fading mode to idle.
        // If the user stops the toDeck during a fade, let the fade continue
        // and do not load the next track.
        if (!otherDeckPlaying && otherDeck->isFromDeck) {
            // Force crossfader all the way to the (non fading) toDeck.
            if (m_eState == ADJ_RIGHT_FADING) {
                setCrossfader(-1.0);
            } else {
                setCrossfader(1.0);
            }
            m_eState = ADJ_IDLE;
            // Invalidate threshold calculated for the old otherDeck
            // This avoids starting a fade back before the new track is
            // loaded into the otherDeck
            thisDeck->fadeBeginPos = 1.0;
            thisDeck->fadeEndPos = 1.0;
            otherDeck->isFromDeck = false;
            // Load the next track to otherDeck.
            loadNextTrackFromQueue(*otherDeck);
            emitAutoDJStateChanged(m_eState);
            return;
        }
    }

    if (m_eState == ADJ_IDLE) {
        if (!thisDeckPlaying && thisPlayPosition < 1) {
            // this is a cueing seek, recalculate the transition, from the
            // new position.
            // This can be our own seek to startPos or a random seek by a user.
            // we need to call calculateTransition() because we are not sure.
            // If using the full track mode with a transition time of 0,
            // thisDeckPlaying will be false but the transition should not be
            // recalculated here.
            // Don't adjust transition when reaching the end. In this case it is
            // always stopped.
            if constexpr (sDebug) {
                qDebug() << this << "playerPositionChanged"
                         << "cueing seek";
            }
            calculateTransition(otherDeck, thisDeck, false);
        } else if (thisDeck->isRepeat()) {
            // repeat pauses auto DJ
            return;
        }
    }

    // If we are past this deck's posThreshold then:
    // - transition into fading mode, play the other deck and fade to it.
    // - check if fading is done and stop the deck
    // - update the crossfader
    if (thisPlayPosition >= thisDeck->fadeBeginPos && thisDeck->isFromDeck && !otherDeck->loading) {
        if (m_eState == ADJ_IDLE) {
            if (thisDeckPlaying || thisPlayPosition >= 1.0) {
                if (shouldUseTandaGap(thisDeck, otherDeck) &&
                        thisDeck->fadeBeginPos >= thisDeck->fadeEndPos) {
                    // The DJ marked this row as a pause point: hand the floor
                    // over rather than starting the next tanda. Checked here,
                    // at the moment the Tanda handoff would begin.
                    if (maybeHoldForAnnouncement(thisDeck)) {
                        return;
                    }
                    startTandaGap(thisDeck, otherDeck);
                    return;
                }
                // Cortina Fade: enter a cortina WITHOUT the standard crossfade to
                // its side, so the crossfader doesn't flick to the cortina and back
                // before the gentle fade-in. Keep it on the outgoing (silent) side;
                // maybeHandleCortinaFade() runs the fade from there. Gated to
                // cortina-fade + Tango, so normal Auto DJ is byte-for-byte unchanged.
                if (m_cortinaFadeEnabled && keepQueueEnabled() &&
                        thisDeck->fadeBeginPos >= thisDeck->fadeEndPos &&
                        isCortina(otherDeck->getLoadedTrack())) {
                    if (!otherDeckPlaying) {
                        otherDeck->play(); // cortina is cued in its silent lead-in
                    }
                    // Stay on the outgoing (silent) side -> no flick.
                    setCrossfader(thisDeck->isLeft() ? -1.0 : 1.0);
                    // Advance the cursor past the cortina.
                    removeLoadedTrackFromTopOfQueue(*otherDeck);
                    thisDeck->stop();
                    thisDeck->isFromDeck = false;
                    thisDeck->fadeBeginPos = 1.0;
                    thisDeck->fadeEndPos = 1.0;
                    // Load the next tanda track into the freed deck (not playing),
                    // so maybeHandleCortinaFade() engages on the cortina's next
                    // callback (ADJ_IDLE + cortina solo) and runs the envelope.
                    loadNextTrackFromQueue(*thisDeck);
                    return; // skip the generic FADING path -> no forced snap
                }

                // The DJ marked this row as a pause point: hand the floor over
                // rather than starting the next tanda. Checked here, at the
                // moment the transition would begin, because stopping once the
                // track has ended is too late - the next one is already audible.
                if (maybeHoldForAnnouncement(thisDeck)) {
                    return;
                }

                // Set the state as FADING.
                m_eState = thisDeck->isLeft() ? ADJ_LEFT_FADING : ADJ_RIGHT_FADING;
                m_transitionProgress = 0.0;
                emitAutoDJStateChanged(m_eState);

                const double toDeckFadeDistance =
                        (thisDeck->fadeEndPos - thisDeck->fadeBeginPos) *
                        getEndSecond(thisDeck) / getEndSecond(otherDeck);
                // Re-cue the track if the user has seeked forward and will miss the fadeBeginPos
                if (otherDeck->playPosition() >= otherDeck->fadeBeginPos - toDeckFadeDistance) {
                    otherDeck->setPlayPosition(otherDeck->startPos);
                }

                if (!otherDeckPlaying) {
                    otherDeck->play();
                }

                if (thisDeck->fadeBeginPos >= thisDeck->fadeEndPos) {
                    setCrossfader(thisDeck->isLeft() ? 1.0 : -1.0);
                }

                // Now that we have started the other deck playing, remove the track
                // that was "on deck" from the top of the queue.
                // Note: This is a DB call and takes long.
                removeLoadedTrackFromTopOfQueue(*otherDeck);
            } else {
                if constexpr (sDebug) {
                    qDebug() << this << "playerPositionChanged()"
                             << pAttributes->group << thisPlayPosition
                             << "but not playing";
                }
            }
        }

        double crossfaderTarget;
        if (m_eState == ADJ_LEFT_FADING) {
            crossfaderTarget = 1.0;
        } else if (m_eState == ADJ_RIGHT_FADING) {
            crossfaderTarget = -1.0;
        } else {
            // this happens if the not playing track is cued into the outro region,
            // calculated for the swapped roles.
            return;
        }

        double currentCrossfader = getCrossfader();

        if (currentCrossfader == crossfaderTarget) {
            // We are done, the fading (from) track is silenced.
            // We don't handle mode switches here since that's handled by
            // the next playerPositionChanged call otherDeck (see the
            // P1/P2FADING case above).
            thisDeck->stop();
            m_transitionProgress = 1.0;
            // Note: If the user has stopped the toDeck during the transition.
            // this deck just stops as well. In this case a stopped AutoDJ is accepted
            // because the use did it intentionally
        } else {
            // We are in Fading state.
            // Calculate the current transitionProgress, the place between begin
            // and end position and the step we have taken since the last call
            double transitionProgress = (thisPlayPosition - thisDeck->fadeBeginPos) /
                    (thisDeck->fadeEndPos - thisDeck->fadeBeginPos);
            double transitionStep = transitionProgress - m_transitionProgress;
            if (transitionStep > 0.0) {
                // We have made progress.
                // Backward seeks pause the transitions; forward seeks speed up
                // the transitions. If there has been a seek beyond endPos, end
                // the transition immediately.
                double remainingCrossfader = crossfaderTarget - currentCrossfader;
                double adjustment = remainingCrossfader /
                        (1.0 - m_transitionProgress) * transitionStep;
                // we move the crossfader linearly with
                // movements in this track's play position.
                setCrossfader(currentCrossfader + adjustment);
            }
            m_transitionProgress = transitionProgress;
            // if we are at 1.0 here, we need an additional callback until the last
            // step is processed and we can stop the deck.
        }
    }
}

TrackPointer AutoDJProcessor::getNextTrackFromQueue() {
    // Get the track at the top of the playlist.
    bool randomQueueEnabled = m_pConfig->getValue<bool>(
            ConfigKey(kPreferenceGroup, QStringLiteral("EnableRandomQueue")));
    int minAutoDJCrateTracks =
            m_pConfig->getValueString(ConfigKey(kPreferenceGroup,
                                              QStringLiteral("RandomQueueMinimumAllowed")))
                    .toInt();
    int tracksToAdd = minAutoDJCrateTracks - m_pAutoDJTableModel->rowCount();
    // In case we start off with < minimum tracks
    if (randomQueueEnabled && (tracksToAdd > 0)) {
        emit randomTrackRequested(tracksToAdd);
    }

    // In Keep Queue mode the next track is at the cursor row instead of the
    // front of the list, so played tracks stay in place. When the cursor
    // reaches the end, getTrack() returns null and Auto DJ stops.
    const int nextRow = keepQueueEnabled() ? m_keepQueueRow : 0;
    while (true) {
        TrackPointer pNextTrack = m_pAutoDJTableModel->getTrack(
                m_pAutoDJTableModel->index(nextRow, 0));

        if (pNextTrack) {
            if (pNextTrack->getFileInfo().checkFileExists()) {
                return pNextTrack;
            } else {
                // Remove missing track from auto DJ playlist. The following row
                // shifts up into nextRow, so the loop re-reads the same index.
                qWarning() << "Auto DJ: Skip missing track" << pNextTrack->getLocation();
                m_pAutoDJTableModel->removeTrack(
                        m_pAutoDJTableModel->index(nextRow, 0));
                // Don't "Requeue" missing tracks to avoid andless loops
                maybeFillRandomTracks();
            }
        } else {
            // We're out of tracks. Return the null TrackPointer.
            return pNextTrack;
        }
    }
}

bool AutoDJProcessor::loadNextTrackFromQueue(const DeckAttributes& deck, bool play) {
    TrackPointer nextTrack = getNextTrackFromQueue();

    // We ran out of tracks in the queue.
    if (!nextTrack) {
        if (anyDeckPlaying()) {
            // The last track is still playing: this call is only looking for a
            // successor to cue up. Stopping here would report the set as over
            // while the floor is still dancing, so stay enabled and let
            // playerPlayChanged() stop us once that track actually ends.
            armStopWhenLastTrackEnds();
            // Eject track (nextTrack is null) as "End of auto DJ warning"
            emitLoadTrackToPlayer(nextTrack, deck.group, false);
            return false;
        }

        // Nothing is playing, so the set really is over: disable AutoDJ.
        toggleAutoDJ(false);

        // And eject track (nextTrack is null) as "End of auto DJ warning"
        emitLoadTrackToPlayer(nextTrack, deck.group, false);
        return false;
    }

    // There is something to play again, so a pending end-of-set stop no longer
    // applies. This matters when tandas are built during a running set: the queue
    // legitimately runs dry for a while, and reanchorKeepQueueCursor() reloads the
    // idle deck as soon as tracks are appended. Leaving the flag set would arm a
    // stop that fires the next time both decks happen to be stopped together.
    m_bStopWhenLastTrackEnds = false;
    emitLoadTrackToPlayer(nextTrack, deck.group, play);
    return true;
}

bool AutoDJProcessor::removeLoadedTrackFromTopOfQueue(const DeckAttributes& deck) {
    if (keepQueueEnabled()) {
        // Keep Queue mode: don't remove the played track, just advance the
        // cursor so the next track plays while this one stays in the list.
        return advanceKeepQueueCursor(deck.getLoadedTrack());
    }
    return removeTrackFromTopOfQueue(deck.getLoadedTrack());
}

bool AutoDJProcessor::removeTrackFromTopOfQueue(TrackPointer pTrack) {
    // No track to test for.
    if (!pTrack) {
        return false;
    }

    TrackId trackId(pTrack->getId());

    // Loaded track is not a library track.
    if (!trackId.isValid()) {
        return false;
    }

    // In Keep Queue mode the "top" of the queue is the cursor row, so undesired
    // tracks (missing, too short, ejected) are removed in place rather than from
    // the front of the list.
    const int row = keepQueueEnabled() ? m_keepQueueRow : 0;

    // Get the track id at the top of the playlist.
    TrackId nextId(m_pAutoDJTableModel->getTrackId(
            m_pAutoDJTableModel->index(row, 0)));

    // No track at the top of the queue.
    if (!nextId.isValid()) {
        return false;
    }

    // If the loaded track is not the next track in the queue then do nothing.
    if (trackId != nextId) {
        return false;
    }

    // Remove the top track.
    m_pAutoDJTableModel->removeTrack(m_pAutoDJTableModel->index(row, 0));

    // Re-queue if configured. Never re-queue in Keep Queue mode.
    if (!keepQueueEnabled() &&
            m_pConfig->getValueString(ConfigKey(kPreferenceGroup, QStringLiteral("Requeue"))).toInt()) {
        m_pAutoDJTableModel->appendTrack(nextId);
    }

    maybeFillRandomTracks();
    return true;
}

int AutoDJProcessor::keepQueueRowForDeck(DeckAttributes* pDeck) {
    if (!pDeck || !keepQueueEnabled()) {
        return -1;
    }
    // The cursor points at the next track, so the playing one sits just before
    // it. Passing that as the guess resolves a repeated cortina to the copy
    // actually on this deck rather than to some other row holding the same file.
    return keepQueueRowForTrack(pDeck->getLoadedTrack(), m_keepQueueRow - 1);
}

void AutoDJProcessor::armStopWhenLastTrackEnds() {
    m_bStopWhenLastTrackEnds = true;
    // Neutralise any pending transition. The deck left without a track must not
    // be faded into, and with nothing on it calculateTransition() would be
    // handed a zero-duration deck.
    for (const auto& pDeck : m_decks) {
        VERIFY_OR_DEBUG_ASSERT(pDeck) {
            continue;
        }
        pDeck->fadeBeginPos = 1.0;
        pDeck->fadeEndPos = 1.0;
        pDeck->isFromDeck = false;
    }
}

bool AutoDJProcessor::shouldStopAfterRow(int row) {
    if (!keepQueueEnabled() || row < 0) {
        return false;
    }
    // An explicit mark is the only reason the set stops, and it applies to any
    // row - a cortina included. Kept as its own function, thin as it is, because
    // both places a transition can be claimed call it: the fade trigger and the
    // cortina hand-off. Those two once made the decision separately and drifted
    // apart, and a cortina played straight on through a stop it should have
    // honoured. Anything added here reaches both by construction.
    return m_pAutoDJTableModel->isPauseAfterRow(row);
}

bool AutoDJProcessor::maybeHoldForAnnouncement(DeckAttributes* pDeck) {
    if (m_bPauseAfterPending) {
        // Already claimed: keep refusing the transition for the rest of the
        // track, which would otherwise re-trigger as the position advances.
        return true;
    }
    if (!keepQueueEnabled() || !pDeck) {
        return false;
    }
    const int row = keepQueueRowForDeck(pDeck);
    if (!shouldStopAfterRow(row)) {
        return false;
    }
    // Consume now, so resuming after the announcement plays on instead of
    // stopping again at the same place.
    m_pAutoDJTableModel->clearPauseAfterRow(row);
    m_pAutoDJTableModel->setActivePauseAfterRow(row);
    m_bPauseAfterPending = true;
    // Let the track finish rather than cutting it: playerPlayChanged() stops
    // Auto DJ once it ends. Neutralise its fade so the transition machinery does
    // not start the next tanda behind our back. The idle deck deliberately keeps
    // its cued track, unlike the end-of-queue path, so resuming is immediate.
    pDeck->fadeBeginPos = 1.0;
    pDeck->fadeEndPos = 1.0;
    updatePauseAfterDeckControl();
    return true;
}

void AutoDJProcessor::updatePauseAfterDeckControl() {
    double deckIndex = 0.0;
    // Deliberately not gated on Auto DJ running: marks are set while it is
    // stopped, which is exactly when the DJ is looking at the decks.
    if (keepQueueEnabled()) {
        for (const auto& pDeck : m_decks) {
            if (!pDeck || !pDeck->getLoadedTrack()) {
                continue;
            }
            const int row = keepQueueRowForDeck(pDeck.get());
            if (row >= 0 &&
                    (m_pAutoDJTableModel->isPauseAfterRow(row) ||
                            (m_bPauseAfterPending && pDeck->isPlaying()))) {
                deckIndex = pDeck->index + 1;
                break;
            }
        }
    }
    if (m_pauseAfterDeck.get() != deckIndex) {
        m_pauseAfterDeck.set(deckIndex);
    }
}

bool AutoDJProcessor::anyDeckPlaying() const {
    for (const auto& pDeck : m_decks) {
        if (pDeck && pDeck->isPlaying()) {
            return true;
        }
    }
    return false;
}

bool AutoDJProcessor::keepQueueEnabled() const {
    return m_keepQueue.toBool();
}

bool AutoDJProcessor::isQueueOrderLocked() const {
    return keepQueueEnabled();
}

bool AutoDJProcessor::liveModeEnabled() const {
    return m_liveMode.toBool();
}

void AutoDJProcessor::disarmStopGuard() {
    if (!m_stopGuardArmed) {
        return;
    }
    m_stopGuardArmed = false;
    m_stopGuardTimer.stop();
    emit stopGuardArmedChanged(false);
}

void AutoDJProcessor::publishHudTiming() {
    if (!keepQueueEnabled() || m_eState == ADJ_DISABLED) {
        m_hudCountdownSeconds.set(-1.0);
        m_hudNextKind.set(keepQueueEnabled() &&
                        m_pAutoDJTableModel &&
                        m_pAutoDJTableModel->hasActivePauseAfterRow()
                        ? 3.0
                        : 0.0);
        return;
    }
    // What comes next, so the label reads "Next track / Cortina / Set ends in".
    // activeKeepQueuePosition() is 1-based; the next item sits at 0-based row ==
    // activePosition.
    bool hasNext = false;
    const int activePosition = activeKeepQueuePosition();
    if (activePosition > 0 && m_pAutoDJTableModel) {
        TrackPointer pNext = m_pAutoDJTableModel->getTrack(
                m_pAutoDJTableModel->index(activePosition, 0));
        if (pNext) {
            hasNext = true;
            m_hudNextKind.set(isCortina(pNext) ? 1.0 : 0.0);
        } else {
            m_hudNextKind.set(2.0); // no next item: the set ends here
        }
    }
    // When the active position can't be resolved (e.g. paused at the end) leave
    // the last kind in place, so the label does not flip away from "Set ends in".

    // Countdown to the moment the next track becomes audible, so "... in mm:ss"
    // stays literally true. Cheap - all values are held by the decks/timers.
    double countdown = -1.0;
    if (m_tandaGapTimer.isActive()) {
        // In the silent gap between two tanda tracks: count the gap itself down.
        countdown = m_tandaGapTimer.remainingTime() / 1000.0;
    } else if (m_cortinaGapTimer.isActive()) {
        countdown = m_cortinaGapTimer.remainingTime() / 1000.0;
    } else if (DeckAttributes* pCortinaDeck = playingCortinaDeck()) {
        // A cortina plays for its effective window (cortina length clamped to the
        // audible span), governed by the fade envelope - not to the file's end.
        // Mirror the envelope's cl - elapsed. Keep in sync with
        // maybeHandleCortinaFade().
        const double duration = getEndSecond(pCortinaDeck);
        if (duration > 0.0) {
            const double envelopeStart =
                    m_cortinaEnvelopeStartSecond != kKeepPosition
                    ? m_cortinaEnvelopeStartSecond
                    : getFirstSoundSecond(pCortinaDeck);
            const double audible = math_max(
                    getLastSoundSecond(pCortinaDeck) - envelopeStart, 0.0);
            const double cl = math_min(
                    static_cast<double>(m_keepQueueCortinaSeconds), audible);
            const double elapsed =
                    pCortinaDeck->playPosition() * duration - envelopeStart;
            countdown = math_max(cl - elapsed, 0.0);
            if (m_cortinaManualFadeOutStartSecond != kKeepPosition) {
                const double manualFadeOutElapsed =
                        pCortinaDeck->playPosition() * duration -
                        m_cortinaManualFadeOutStartSecond;
                const double manualFadeOutSeconds = math_min(
                        static_cast<double>(m_cortinaFadeOutSeconds),
                        math_max(getLastSoundSecond(pCortinaDeck) -
                                        m_cortinaManualFadeOutStartSecond,
                                0.0));
                countdown = math_max(
                        manualFadeOutSeconds - manualFadeOutElapsed,
                        0.0);
            }
            // Plus the silent after-gap before the next tanda track starts.
            if (hasNext) {
                countdown += m_cortinaGapSeconds;
            }
        }
    } else {
        // The main track's remaining. Prefer the flagged from-deck, but fall back
        // to any playing non-cortina deck so the last track - which has no
        // transition pending and hence no from-deck - still shows a countdown.
        // Paused decks show --:-- (a frozen number would mislead).
        DeckAttributes* pDeck = getFromDeck();
        if (!pDeck || !pDeck->isPlaying()) {
            pDeck = nullptr;
            for (const auto& pCandidate : m_decks) {
                if (pCandidate && pCandidate->isPlaying() &&
                        !isCortina(pCandidate->getLoadedTrack())) {
                    pDeck = pCandidate.get();
                    break;
                }
            }
        }
        if (pDeck && pDeck->isPlaying()) {
            TrackPointer pTrack = pDeck->getLoadedTrack();
            const double pos = pDeck->playPosition();
            if (pTrack && pos >= 0.0) {
                double remaining =
                        keepQueueCurrentTrackRemainingSeconds(pTrack, pos);
                if (hasNext) {
                    if (m_transitionMode == TransitionMode::TandaTransition) {
                        // Hard cut, then a silent gap before the next item.
                        remaining += m_tandaGapSeconds;
                    } else {
                        // Crossfade modes: the incoming becomes audible when the
                        // crossfade starts, m_transitionTime before the end.
                        remaining -= m_transitionTime;
                    }
                }
                countdown = math_max(remaining, 0.0);
            }
        }
    }
    m_hudCountdownSeconds.set(countdown);
}

void AutoDJProcessor::setHudTandaState(int trackCount, int playingIndex) {
    m_hudTandaTrackCount.set(trackCount);
    m_hudTandaPlayingIndex.set(playingIndex);
}

mixxx::Duration AutoDJProcessor::getRemainingSetDuration() {
    if (!keepQueueEnabled()) {
        return mixxx::Duration::empty();
    }
    // The upcoming-tracks sum is expensive (it reads each queued track from the
    // database to inspect its audible range), but it only changes on queue/cursor/
    // mode edits. Recompute it lazily and serve the cached value to the 1 Hz UI
    // refresh; only the cheap current-track remainder below is computed per call.
    refreshSetDurationCacheIfNeeded();
    double seconds = m_keepQueueUpcomingSeconds;
    int remainingTracks = m_keepQueueUpcomingTracks;
    // Add the unplayed remainder of the current (playing or paused) track. The
    // loaded track is held by the deck, so this is cheap (no database read). While
    // paused this stays frozen, so "time left" holds steady and the projected end
    // clock slips later in real time, which is the desired behaviour.
    // In stock fixed modes with Cortina Fade enabled, the boundaries into and
    // out of each cortina use the Nc gaps counted per cortina in the sums, so
    // exclude them from the stock transition-time adjustment below. Tanda
    // Transition accounts for every boundary with its own positive gap instead.
    int cortinaBoundaries =
            m_transitionMode != TransitionMode::TandaTransition && m_cortinaFadeEnabled
            ? 2 * m_keepQueueUpcomingCortinas
            : 0;
    if (m_eState != ADJ_DISABLED) {
        DeckAttributes* pFromDeck = getFromDeck();
        if (pFromDeck) {
            TrackPointer pTrack = pFromDeck->getLoadedTrack();
            const double pos = pFromDeck->playPosition();
            if (pTrack && pos >= 0.0) {
                seconds += keepQueueCurrentTrackRemainingSeconds(pTrack, pos);
                remainingTracks += 1;
                if (m_transitionMode != TransitionMode::TandaTransition &&
                        m_cortinaFadeEnabled && isCortina(pTrack)) {
                    // Only the boundary out of the playing cortina remains.
                    cortinaBoundaries += 1;
                }
            }
        }
    }
    // Account for the configured gap (negative transition time) or crossfade
    // overlap (positive) at each remaining track boundary. Only the fixed modes
    // use a deterministic transition time; the intro/outro modes depend on
    // per-track cues we cannot know in advance, so the adjustment is skipped there.
    if (m_transitionMode == TransitionMode::TandaTransition) {
        const int transitions = remainingTracks > 1 ? remainingTracks - 1 : 0;
        seconds += transitions * m_tandaGapSeconds;
    } else if (m_transitionMode == TransitionMode::FixedFullTrack ||
            m_transitionMode == TransitionMode::FixedSkipSilence) {
        int transitions = remainingTracks > 1 ? remainingTracks - 1 : 0;
        transitions -= cortinaBoundaries;
        if (transitions < 0) {
            transitions = 0;
        }
        seconds -= transitions * m_transitionTime;
    }
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    return mixxx::Duration::fromMillis(static_cast<qint64>(seconds * 1000.0));
}

void AutoDJProcessor::refreshSetDurationCacheIfNeeded() {
    // The cortina budget is baked into the cached sums, so a change to it (only
    // possible while Auto DJ is stopped) must re-dirty the cache.
    const int cortinaSeconds =
            m_pConfig->getValue(ConfigKey(kPreferenceGroup,
                                        QStringLiteral("CortinaLength")),
                    45);
    if (cortinaSeconds != m_keepQueueCortinaSeconds) {
        m_keepQueueCortinaSeconds = cortinaSeconds;
        m_keepQueueDurationDirty = true;
    }
    // The Cortina Fade settings feed both the engine and the estimate. While a
    // set is running they stay frozen at the toggleAutoDJ() snapshot; while
    // stopped, pick up preference edits here so the Set Length preview follows
    // the prefs dialog. Fade mode and the unified Tanda gap are baked into the
    // cached sums, so a change to them re-dirties the cache.
    if (m_eState == ADJ_DISABLED) {
        const bool fadeEnabled = m_cortinaFadeEnabled;
        const int gapSeconds = m_cortinaGapSeconds;
        const int tandaGapSeconds = m_tandaGapSeconds;
        m_tandaGapSeconds = m_pConfig->getValue(
                ConfigKey(kPreferenceGroup, kTandaGapPreferenceName),
                kTandaGapPreferenceDefault);
        loadCortinaFadeSettings();
        if (fadeEnabled != m_cortinaFadeEnabled ||
                gapSeconds != m_cortinaGapSeconds ||
                tandaGapSeconds != m_tandaGapSeconds) {
            m_keepQueueDurationDirty = true;
        }
    }
    if (m_keepQueueDurationDirty) {
        recomputeKeepQueueUpcomingDuration();
    }
}

void AutoDJProcessor::loadCortinaFadeSettings() {
    m_cortinaFadeEnabled =
            m_pConfig->getValue(ConfigKey(kPreferenceGroup,
                                        QStringLiteral("CortinaFadeMode")),
                    0) == 1;
    m_cortinaFadeInSeconds =
            m_pConfig->getValue(ConfigKey(kPreferenceGroup,
                                        QStringLiteral("CortinaFadeIn")),
                    5);
    m_cortinaFadeOutSeconds =
            m_pConfig->getValue(ConfigKey(kPreferenceGroup,
                                        QStringLiteral("CortinaFadeOut")),
                    5);
    // Tanda Transition exposes a single gap value. Keep the internal cortina-gap
    // member as the future split point, but drive it from that unified setting.
    m_cortinaGapSeconds = m_tandaGapSeconds;
}

bool AutoDJProcessor::isCortina(const TrackPointer& pTrack) const {
    return pTrack &&
            CortinaRegistry::instance().contains(TrackId(pTrack->getId()));
}

DeckAttributes* AutoDJProcessor::playingCortinaDeck() const {
    for (const auto& pDeck : m_decks) {
        if (pDeck && pDeck->isPlaying() && isCortina(pDeck->getLoadedTrack())) {
            return pDeck.get();
        }
    }
    return nullptr;
}

bool AutoDJProcessor::maybeHandleCortinaFade(
        DeckAttributes* thisDeck, double thisPlayPosition) {
    // Cortina Fade is a Tango (Keep Queue) feature; normal Auto DJ must stay
    // completely unaffected even if cortina marks linger from a Tango session.
    if (!keepQueueEnabled() ||
            (!m_cortinaFadeEnabled &&
                    m_cortinaFadePhase == CortinaFadePhase::None)) {
        if (m_cortinaFadePhase != CortinaFadePhase::None) {
            qInfo().nospace() << "[CORTINA_ENVELOPE] reject reason=keepQueueOff"
                              << " deck=" << thisDeck->group
                              << " keepQueue=" << yesNo(keepQueueEnabled())
                              << " fadeMode=" << yesNo(m_cortinaFadeEnabled)
                              << " phase=" << static_cast<int>(m_cortinaFadePhase);
        }
        return false;
    }

    if (m_cortinaFadePhase == CortinaFadePhase::None) {
        // Not active: consider taking over for a cortina playing solo while
        // idle. In any other state (enabling, an active normal fade) the
        // standard transition machinery is in charge and must not be
        // second-guessed.
        if (m_eState != ADJ_IDLE || !thisDeck->isPlaying()) {
            return false;
        }
        const TrackPointer pCortina = thisDeck->getLoadedTrack();
        if (!isCortina(pCortina)) {
            return false;
        }
        DeckAttributes* pNextDeck = getOtherDeck(thisDeck);
        if (!pNextDeck || pNextDeck->isPlaying()) {
            return false;
        }
        const double duration = getEndSecond(thisDeck);
        if (duration < kMinimumTrackDurationSec) {
            return false;
        }
        m_pCortinaDeck = thisDeck;
        m_cortinaTrackId = TrackId(pCortina->getId());

        // Insert the Nc before-gap only while the cortina is still inside its
        // silent lead-in (calculateTransition() cues it Nc ahead of the first
        // sound, so the hard cut onto it and these first callbacks are
        // inaudible). If it is already audible - e.g. Auto DJ was enabled mid-
        // cortina - a pause/seek-back would be an audible stutter, so skip the
        // gap and run the envelope from the current position instead.
        const double firstSound = getFirstSoundSecond(thisDeck);
        const int gapSeconds = m_transitionMode == TransitionMode::TandaTransition
                ? m_tandaGapSeconds
                : m_cortinaGapSeconds;
        TT_TRACE() << "cortina phase decision deck=" << thisDeck->group
                   << " playSecond=" << (thisPlayPosition * duration)
                   << " firstSound=" << firstSound
                   << " gap=" << gapSeconds;
        if (thisPlayPosition * duration <= firstSound + 0.1) {
            m_cortinaFadePhase = CortinaFadePhase::BeforeGap;
            m_cortinaEnvelopeStartSecond = firstSound;
            TT_TRACE() << "cortina phase=BeforeGap deck=" << thisDeck->group
                       << " seekSecond=" << firstSound
                       << " gapMs=" << (gapSeconds * 1000);
            // Order matters to keep this inaudible: stop and silence the
            // cortina before seeking it onto its first audible sample.
            thisDeck->stop();
            setCrossfader(thisDeck->isLeft() ? -1.0 : 1.0);
            setAutoDJFadeGain(thisDeck, 0.0);
            thisDeck->setPlayPosition(firstSound / duration);
            m_cortinaGapTimer.start(gapSeconds * 1000);
            return true;
        }
        m_cortinaFadePhase = CortinaFadePhase::Envelope;
        m_cortinaEnvelopeStartSecond = firstSound;
        setCrossfader(thisDeck->isLeft() ? -1.0 : 1.0);
        TT_TRACE() << "cortina phase=Envelope deck=" << thisDeck->group
                   << " reason=already-audible";
        // Fall through to the envelope below.
    } else {
        // An active phase owns the cortina deck's callbacks outright, so the
        // normal machinery can't reinterpret our own seeks and stops. The
        // other deck's callbacks are not claimed.
        if (thisDeck != m_pCortinaDeck) {
            qInfo().nospace() << "[CORTINA_ENVELOPE] ignoreOtherDeck deck="
                              << thisDeck->group
                              << " owner="
                              << (m_pCortinaDeck ? m_pCortinaDeck->group
                                                : QStringLiteral("<none>"))
                              << " phase=" << static_cast<int>(m_cortinaFadePhase);
            return false;
        }
        const TrackPointer pTrack = thisDeck->getLoadedTrack();
        if (m_eState == ADJ_DISABLED || !pTrack ||
                TrackId(pTrack->getId()) != m_cortinaTrackId) {
            // The world changed under us (state change, eject, manual load):
            // relinquish control instead of driving the wrong track.
            TT_TRACE() << "cortina cancel deck=" << thisDeck->group
                       << " reason=state-or-track-changed";
            qInfo().nospace() << "[CORTINA_ENVELOPE] cancel reason=stateOrTrackChanged"
                              << " deck=" << thisDeck->group
                              << " state=" << static_cast<int>(m_eState)
                              << " hasTrack=" << yesNo(static_cast<bool>(pTrack))
                              << " trackId="
                              << (pTrack ? TrackId(pTrack->getId()).toString()
                                         : QStringLiteral("<none>"))
                              << " expectedTrackId=" << m_cortinaTrackId.toString()
                              << " phase=" << static_cast<int>(m_cortinaFadePhase);
            cancelCortinaFade();
            return false;
        }
        if (m_cortinaFadePhase != CortinaFadePhase::Envelope) {
            // Waiting out a gap with the cortina deck stopped. Swallow stray
            // callbacks (e.g. from our own seek) so the "cueing seek" logic
            // doesn't recalculate transitions meanwhile.
            return true;
        }
    }

    // Envelope: drive the internal Auto DJ gain as a pure function of the seconds elapsed
    // since the cortina envelope entry point.
    const double duration = getEndSecond(thisDeck);
    const double firstSound = getFirstSoundSecond(thisDeck);
    const double envelopeStart = m_cortinaEnvelopeStartSecond != kKeepPosition
            ? m_cortinaEnvelopeStartSecond
            : firstSound;
    const double elapsed = thisPlayPosition * duration - envelopeStart;

    // Envelope lengths. Clamp the on-deck budget (Cl) to the cortina's audible
    // span, and if fade-in + fade-out still overflow it, scale them down so the
    // hold time Y stays >= 0.
    const double audible =
            math_max(getLastSoundSecond(thisDeck) - envelopeStart, 0.0);
    const double cl = math_min(
            static_cast<double>(m_keepQueueCortinaSeconds), audible);
    double x = static_cast<double>(m_cortinaFadeInSeconds);
    double z = static_cast<double>(m_cortinaFadeOutSeconds);
    if (x + z > cl) {
        const double scale = (x + z > 0.0) ? cl / (x + z) : 0.0;
        x *= scale;
        z *= scale;
    }

    if (!thisDeck->isPlaying()) {
        if (thisPlayPosition >= 1.0 || elapsed >= cl) {
            // The file ended before the envelope + after-gap completed (short
            // cortina / no silent tail). Go straight to the after-gap so the
            // handoff to the next tanda still happens.
            TT_TRACE() << "cortina phase=AfterGap deck=" << thisDeck->group
                       << " reason=file-ended";
            startCortinaAfterGap(thisDeck);
            return true;
        }
        // Paused mid-envelope by the DJ: they are taking over.
        TT_TRACE() << "cortina cancel deck=" << thisDeck->group
                   << " reason=paused-mid-envelope";
        qInfo().nospace() << "[CORTINA_ENVELOPE] cancel reason=pausedMidEnvelope"
                          << " deck=" << thisDeck->group
                          << " playpos=" << thisPlayPosition
                          << " elapsed=" << elapsed
                          << " cl=" << cl;
        cancelCortinaFade();
        return false;
    }

    // The crossfader stays parked on the cortina side. The audible cortina
    // envelope is an internal Auto DJ gain so silent-gap crossfader movement can
    // be used consistently as handoff progress feedback.
    setCrossfader(thisDeck->isLeft() ? -1.0 : 1.0);

    if (m_cortinaManualFadeOutStartSecond != kKeepPosition) {
        const double manualFadeOutElapsed =
                thisPlayPosition * duration - m_cortinaManualFadeOutStartSecond;
        const double manualFadeOutSeconds = math_min(
                static_cast<double>(m_cortinaFadeOutSeconds),
                math_max(getLastSoundSecond(thisDeck) -
                                m_cortinaManualFadeOutStartSecond,
                        0.0));
        double gain = 0.0;
        if (manualFadeOutElapsed < 0.0) {
            gain = m_cortinaManualFadeOutStartGain;
        } else if (manualFadeOutElapsed < manualFadeOutSeconds &&
                manualFadeOutSeconds > 0.0) {
            gain = m_cortinaManualFadeOutStartGain *
                    (1.0 - (manualFadeOutElapsed / manualFadeOutSeconds));
        } else {
            // Fade-out complete: enter the Nc after-gap.
            TT_TRACE() << "cortina phase=AfterGap deck=" << thisDeck->group
                       << " reason=manual-envelope-complete";
            setAutoDJFadeGain(thisDeck, 0.0);
            qInfo().nospace()
                    << "[CORTINA_ENVELOPE] complete deck=" << thisDeck->group
                    << " reason=manual"
                    << " playpos=" << thisPlayPosition
                    << " duration=" << duration
                    << " manualFadeOutElapsed=" << manualFadeOutElapsed
                    << " manualFadeOutSeconds=" << manualFadeOutSeconds
                    << " manualStartGain=" << m_cortinaManualFadeOutStartGain;
            startCortinaAfterGap(thisDeck);
            return true;
        }
        setAutoDJFadeGain(thisDeck, gain);
        qInfo().nospace() << "[CORTINA_ENVELOPE] gain deck=" << thisDeck->group
                          << " reason=manual"
                          << " playpos=" << thisPlayPosition
                          << " duration=" << duration
                          << " manualFadeOutStartSecond="
                          << m_cortinaManualFadeOutStartSecond
                          << " manualFadeOutElapsed=" << manualFadeOutElapsed
                          << " manualFadeOutSeconds=" << manualFadeOutSeconds
                          << " manualStartGain="
                          << m_cortinaManualFadeOutStartGain
                          << " gain=" << gain
                          << " phase=" << static_cast<int>(m_cortinaFadePhase)
                          << " crossfader=" << getCrossfader();
        return true;
    }

    double gain;
    if (elapsed < 0.0) {
        // Still inside the silent lead-in.
        gain = 0.0;
    } else if (elapsed < x) {
        // Fade in.
        gain = x > 0.0 ? elapsed / x : 1.0;
    } else if (elapsed < cl - z) {
        // Hold at full.
        gain = 1.0;
    } else if (elapsed < cl) {
        // Fade out.
        gain = z > 0.0 ? 1.0 - ((elapsed - (cl - z)) / z) : 0.0;
    } else {
        // Fade-out complete: enter the Nc after-gap.
        TT_TRACE() << "cortina phase=AfterGap deck=" << thisDeck->group
                   << " reason=envelope-complete";
        setAutoDJFadeGain(thisDeck, 0.0);
        qInfo().nospace() << "[CORTINA_ENVELOPE] complete deck=" << thisDeck->group
                          << " playpos=" << thisPlayPosition
                          << " duration=" << duration
                          << " elapsed=" << elapsed
                          << " cl=" << cl
                          << " x=" << x
                          << " z=" << z;
        startCortinaAfterGap(thisDeck);
        return true;
    }
    setAutoDJFadeGain(thisDeck, gain);
    qInfo().nospace() << "[CORTINA_ENVELOPE] gain deck=" << thisDeck->group
                      << " playpos=" << thisPlayPosition
                      << " duration=" << duration
                      << " envelopeStart=" << envelopeStart
                      << " elapsed=" << elapsed
                      << " cl=" << cl
                      << " x=" << x
                      << " z=" << z
                      << " gain=" << gain
                      << " phase=" << static_cast<int>(m_cortinaFadePhase)
                      << " crossfader=" << getCrossfader();
    return true;
}

void AutoDJProcessor::startCortinaAfterGap(DeckAttributes* pCortinaDeck) {
    const int gapSeconds = m_transitionMode == TransitionMode::TandaTransition
            ? m_tandaGapSeconds
            : m_cortinaGapSeconds;
    TT_TRACE() << "cortina startAfterGap deck=" << pCortinaDeck->group
               << " gapMs=" << (gapSeconds * 1000);
    m_cortinaFadePhase = CortinaFadePhase::AfterGap;
    m_cortinaManualFadeOutStartSecond = kKeepPosition;
    m_cortinaManualFadeOutStartGain = 1.0;
    // Stop the now-inaudible cortina and restore its internal gain while it is
    // silent, so the deck is safe if the DJ takes over or the track is reloaded.
    pCortinaDeck->stop();
    setAutoDJFadeGain(pCortinaDeck, 1.0);
    const int gapMs = gapSeconds * 1000;
    if (m_transitionMode == TransitionMode::TandaTransition) {
        startTandaCrossfaderAnimation(pCortinaDeck, getOtherDeck(pCortinaDeck), gapMs);
    } else {
        setCrossfader(pCortinaDeck->isLeft() ? 1.0 : -1.0);
    }
    m_cortinaGapTimer.start(gapMs);
}

void AutoDJProcessor::cancelCortinaFade() {
    TT_TRACE() << "cortina cancel phase=" << static_cast<int>(m_cortinaFadePhase);
    m_cortinaGapTimer.stop();
    stopTandaCrossfaderAnimation();
    if (m_pCortinaDeck) {
        setAutoDJFadeGain(m_pCortinaDeck, 1.0);
    }
    m_cortinaFadePhase = CortinaFadePhase::None;
    m_cortinaEnvelopeStartSecond = kKeepPosition;
    m_cortinaManualFadeOutStartSecond = kKeepPosition;
    m_cortinaManualFadeOutStartGain = 1.0;
    m_pCortinaDeck = nullptr;
    m_cortinaTrackId = TrackId();
}

void AutoDJProcessor::slotCortinaGapElapsed() {
    TT_TRACE() << "cortina gap elapsed phase=" << static_cast<int>(m_cortinaFadePhase);
    DeckAttributes* pCortinaDeck = m_pCortinaDeck;
    if (!pCortinaDeck || m_eState != ADJ_IDLE) {
        cancelCortinaFade();
        return;
    }

    if (m_cortinaFadePhase == CortinaFadePhase::BeforeGap) {
        // If the track on the gapped deck changed while the timer ran (eject or
        // a manual load), blindly starting the deck would play the wrong track
        // with the crossfader parked on its silent side. Relinquish control.
        const TrackPointer pTrack = pCortinaDeck->getLoadedTrack();
        if (!pTrack || TrackId(pTrack->getId()) != m_cortinaTrackId) {
            cancelCortinaFade();
            return;
        }
        // The before-gap has elapsed; resume the cortina. The position handler
        // then ramps the internal Auto DJ gain in from silence.
        m_cortinaFadePhase = CortinaFadePhase::Envelope;
        pCortinaDeck->play();
        return;
    }

    if (m_cortinaFadePhase == CortinaFadePhase::AfterGap) {
        // If the DJ touched the cortina deck during the gap (restart, eject,
        // manual load), they are taking over: don't force the handoff.
        const TrackPointer pCortinaTrack = pCortinaDeck->getLoadedTrack();
        if (pCortinaDeck->isPlaying() || pCortinaDeck->loading ||
                !pCortinaTrack ||
                TrackId(pCortinaTrack->getId()) != m_cortinaTrackId) {
            cancelCortinaFade();
            return;
        }
        DeckAttributes* pNextDeck = getOtherDeck(pCortinaDeck);
        if (!pNextDeck) {
            cancelCortinaFade();
            return;
        }
        // End-of-set check comes first, before the loading guard below. When the
        // queue has no track after this cortina the set is over - even if a played
        // track lingers on the other deck (keep-queue removes nothing on play). At
        // end of set loadNextTrackFromQueue() ejects a null "end of Auto DJ" track
        // onto the idle deck, which leaves it flagged loading forever; the loading
        // guard would then re-arm the gap timer every 500ms and Auto DJ would never
        // stop. Consult the authoritative cursor first so the set actually ends.
        if (!getNextTrackFromQueue()) {
            if (!pNextDeck->isPlaying()) {
                // No next track and the DJ hasn't taken over: complete the set.
                cancelCortinaFade();
                m_bStopWhenLastTrackEnds = true;
                toggleAutoDJ(false);
                return;
            }
            // The DJ started a deck themselves: back off and let them drive.
            cancelCortinaFade();
            return;
        }
        if (pNextDeck->loading) {
            // The next track is still loading (e.g. Auto DJ was only just
            // enabled): extend the gap slightly rather than stalling the set
            // with both decks stopped.
            TT_TRACE() << "cortina gap extend reason=next-loading gapMs=500";
            m_cortinaGapTimer.start(500);
            return;
        }
        const double nextDuration = getEndSecond(pNextDeck);
        if (pNextDeck->isPlaying() || nextDuration < kMinimumTrackDurationSec) {
            // The queue has a next track but nothing playable is cued here (or the
            // DJ started a deck): back off.
            cancelCortinaFade();
            return;
        }
        // A cortina the DJ marked for an announcement keeps its whole envelope:
        // the before-gap, fade-in, hold and fade-out have all run by the time we
        // get here, and only then does the set stop. Nothing is playing at this
        // point, so stop directly rather than waiting for a deck to end.
        const int cortinaRow = keepQueueRowForDeck(pCortinaDeck);
        if (shouldStopAfterRow(cortinaRow)) {
            m_pAutoDJTableModel->clearPauseAfterRow(cortinaRow);
            m_pAutoDJTableModel->setActivePauseAfterRow(cortinaRow);
            cancelCortinaFade();
            // Marks this as an automatic stop, so the LIVE guard lets it through
            // instead of waiting for a confirmation that will never come.
            m_bPauseAfterPending = true;
            toggleAutoDJ(false);
            return;
        }

        // Reset the phase before starting the next track: from here on its
        // callbacks belong to the normal transition machinery again.
        cancelCortinaFade();
        // The Nc gap was the silent hold above, so cue the next track to its
        // first audible sample (no extra pre-roll) and hard-start it; the
        // crossfader is already parked on its side, so it comes in at full.
        // Except when it is itself a cortina: keep its silent lead-in so its
        // own before-gap can engage without an audible blip.
        const TrackPointer pNextTrack = pNextDeck->getLoadedTrack();
        double startSecond = getFirstSoundSecond(pNextDeck);
        if (m_transitionMode == TransitionMode::TandaTransition) {
            startSecond = tandaEntryPointSecond(pNextDeck);
        } else if (isCortina(pNextTrack)) {
            startSecond = math_max(
                    startSecond - static_cast<double>(m_cortinaGapSeconds), 0.0);
        }
        TT_TRACE() << "cortina next start deck=" << pNextDeck->group
                   << " startSecond=" << startSecond
                   << " nextIsCortina=" << yesNo(isCortina(pNextTrack));
        pNextDeck->setPlayPosition(startSecond / nextDuration);
        pNextDeck->play();
        removeLoadedTrackFromTopOfQueue(*pNextDeck);
        // Load the next tanda track into the freed cortina deck;
        // playerTrackLoaded() then arms the next (normal) transition.
        loadNextTrackFromQueue(*pCortinaDeck);
    }
}

void AutoDJProcessor::recomputeKeepQueueUpcomingDuration() {
    const int rowCount = m_pAutoDJTableModel->rowCount();
    double totalSeconds = 0.0;
    double upcomingSeconds = 0.0;
    int totalCortinas = 0;
    int upcomingCortinas = 0;
    // Counted in the loop rather than from the row count, since intro and outro
    // rows are skipped entirely.
    int totalTracks = 0;
    int upcomingTracks = 0;
    // One database pass feeds both caches: the whole-queue total (Set Length) and
    // the cursor-onward remainder (used by the time-left/end estimate).
    //
    // Deliberately does not call getTrack() here. That constructs a full Track,
    // which imports metadata from the file -- an fopen plus a TagLib parse per
    // row, on the GUI thread. A queue of any size then blocks the UI for as long
    // as it takes to read every file (a full-library queue freezes Mixxx for
    // minutes). Everything this loop needs is already to hand: the duration comes
    // from the database column the model is backed by, and a cortina is
    // identified by TrackId alone.
    //
    // This mirrors keepQueueTrackPlaySeconds() without the Track: keep the two in
    // sync, since they must agree on what a queued track costs.
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex index = m_pAutoDJTableModel->index(row, 0);
        const TrackId trackId = m_pAutoDJTableModel->getTrackId(index);
        double seconds = m_pAutoDJTableModel->durationSecondsForRow(row);
        const bool cortina = trackId.isValid() &&
                CortinaRegistry::instance().contains(trackId);

        if (cortina) {
            seconds = std::min<double>(m_keepQueueCortinaSeconds, seconds);
            if (m_transitionMode != TransitionMode::TandaTransition &&
                    m_cortinaFadeEnabled) {
                // Stock Cortina Fade inserts a silent Nc gap before and after
                // the cortina, so the queue costs both gaps on top of the budget.
                seconds += 2.0 * m_cortinaGapSeconds;
            }
        } else if (m_transitionMode == TransitionMode::FixedSkipSilence ||
                m_transitionMode == TransitionMode::TandaTransition) {
            // The audible-range refinement needs the analysed N60dBSound cue,
            // which only exists on a loaded Track. Consult one only if it is
            // already in memory -- loading it here is exactly the cost this loop
            // must avoid, and an unanalysed track falls back to the full duration
            // anyway, which is what the database column already gives us.
            const TrackPointer pCached =
                    GlobalTrackCacheLocker().lookupTrackById(trackId);
            if (pCached) {
                const double audible = keepQueueAudibleSeconds(pCached);
                if (audible > 0.0) {
                    seconds = audible;
                }
            }
        }


        totalSeconds += seconds;
        totalCortinas += cortina ? 1 : 0;
        ++totalTracks;
        if (row >= m_keepQueueRow) {
            upcomingSeconds += seconds;
            upcomingCortinas += cortina ? 1 : 0;
            ++upcomingTracks;
        }
    }
    m_keepQueueUpcomingSeconds = upcomingSeconds;
    m_keepQueueUpcomingTracks = upcomingTracks;
    m_keepQueueTotalSeconds = totalSeconds;
    m_keepQueueTotalTracks = totalTracks;
    m_keepQueueUpcomingCortinas = upcomingCortinas;
    m_keepQueueTotalCortinas = totalCortinas;
    m_keepQueueDurationDirty = false;
}

mixxx::Duration AutoDJProcessor::getTotalSetDuration() {
    if (!keepQueueEnabled()) {
        return mixxx::Duration::empty();
    }
    refreshSetDurationCacheIfNeeded();
    double seconds = m_keepQueueTotalSeconds;
    // Subtract the per-boundary gap / crossfade overlap, matching the remaining
    // duration estimate. Only the fixed modes have a deterministic transition
    // time. In Cortina Fade mode the boundaries around each cortina use the Nc
    // gaps (counted per cortina in the sums) instead of Nt, so exclude them.
    if (m_transitionMode == TransitionMode::TandaTransition) {
        const int transitions =
                m_keepQueueTotalTracks > 1 ? m_keepQueueTotalTracks - 1 : 0;
        seconds += transitions * m_tandaGapSeconds;
    } else if (m_transitionMode == TransitionMode::FixedFullTrack ||
            m_transitionMode == TransitionMode::FixedSkipSilence) {
        int transitions =
                m_keepQueueTotalTracks > 1 ? m_keepQueueTotalTracks - 1 : 0;
        if (m_cortinaFadeEnabled) {
            transitions -= 2 * m_keepQueueTotalCortinas;
            if (transitions < 0) {
                transitions = 0;
            }
        }
        seconds -= transitions * m_transitionTime;
    }
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    return mixxx::Duration::fromMillis(static_cast<qint64>(seconds * 1000.0));
}

double AutoDJProcessor::keepQueueAudibleSeconds(const TrackPointer& pTrack) const {
    if (!pTrack) {
        return 0.0;
    }
    CuePointer pCue = pTrack->findCueByType(mixxx::CueType::N60dBSound);
    if (!pCue) {
        return 0.0;
    }
    const mixxx::audio::SampleRate sampleRate = pTrack->getSampleRate();
    const double frames = pCue->getLengthFrames();
    if (!sampleRate.isValid() || frames <= 0.0) {
        return 0.0;
    }
    return frames / sampleRate.toDouble();
}

double AutoDJProcessor::keepQueueTrackPlaySeconds(const TrackPointer& pTrack) const {
    if (!pTrack) {
        return 0.0;
    }
    // The estimate budgets only the configured cortina length for a cortina: in
    // Cortina Fade mode that is the on-deck envelope length; otherwise the DJ
    // fades it out manually around that budget. Cap at the file length so a
    // hand-picked cortina shorter than the budget isn't over-counted.
    if (isCortina(pTrack)) {
        double seconds = std::min<double>(
                m_keepQueueCortinaSeconds, pTrack->getDuration());
        if (m_transitionMode != TransitionMode::TandaTransition &&
                m_cortinaFadeEnabled) {
            // Stock Cortina Fade inserts a silent Nc gap before and after the
            // cortina. Tanda Transition accounts for gaps per boundary instead.
            seconds += 2.0 * m_cortinaGapSeconds;
        }
        return seconds;
    }
    // In Skip Silence mode the engine plays only the audible range, so subtract the
    // trimmed leading/trailing silence using the analyzed N60dBSound cue. Tracks
    // not yet analyzed have no such cue, so fall back to the full file duration.
    if (m_transitionMode == TransitionMode::FixedSkipSilence ||
            m_transitionMode == TransitionMode::TandaTransition) {
        const double audible = keepQueueAudibleSeconds(pTrack);
        if (audible > 0.0) {
            return audible;
        }
    }
    return pTrack->getDuration();
}

double AutoDJProcessor::keepQueueCurrentTrackRemainingSeconds(
        const TrackPointer& pTrack, double playPosition) const {
    const double fullSeconds = pTrack->getDuration();
    const double currentSeconds = fullSeconds * playPosition;
    // Cortina: budget a fixed play time using the ACTUAL elapsed position. While
    // the DJ stays within budget the projected end clock holds steady; once they
    // ride the cortina past the budget the remainder floors at 0 and the end clock
    // slips later in real time, which is the correct, self-correcting behaviour.
    if (isCortina(pTrack)) {
        // Cap at the file length: the deck can't play past the end even if the
        // budget is longer than the track.
        const double budget = std::min<double>(
                m_keepQueueCortinaSeconds, fullSeconds);
        const double remaining = budget - currentSeconds;
        double seconds = remaining > 0.0 ? remaining : 0.0;
        if (m_transitionMode != TransitionMode::TandaTransition &&
                m_cortinaFadeEnabled) {
            // The Nc after-gap is still to come (the before-gap has passed).
            seconds += m_cortinaGapSeconds;
        }
        return seconds;
    }
    // In Skip Silence mode the current track fades out at its last audible sample,
    // not the end of the file, so count the remainder up to that point.
    if (m_transitionMode == TransitionMode::FixedSkipSilence ||
            m_transitionMode == TransitionMode::TandaTransition) {
        CuePointer pCue = pTrack->findCueByType(mixxx::CueType::N60dBSound);
        const mixxx::audio::SampleRate sampleRate = pTrack->getSampleRate();
        if (pCue && sampleRate.isValid() && pCue->getEndPosition().isValid()) {
            const double lastSoundSeconds =
                    pCue->getEndPosition().value() / sampleRate.toDouble();
            const double remaining = lastSoundSeconds - currentSeconds;
            return remaining > 0.0 ? remaining : 0.0;
        }
    }
    const double remaining = fullSeconds - currentSeconds;
    return remaining > 0.0 ? remaining : 0.0;
}

void AutoDJProcessor::controlKeepQueue(double value) {
    const bool enabled = value > 0.0;
    resetAllAutoDJFadeGains();
    stopTandaCrossfaderAnimation();
    const ConfigKey keepQueueKey(kPreferenceGroup, QStringLiteral("KeepQueue"));
    const bool wasEnabled = m_pConfig->getValue(keepQueueKey, false);
    if (m_eState == ADJ_DISABLED) {
        if (enabled && !wasEnabled) {
            setTransitionMode(TransitionMode::TandaTransition);
        } else if (!enabled && wasEnabled && isTandaTransition(m_transitionMode)) {
            restoreLastStockTransitionMode();
        }
    }
    // Persist the live Tango DJ mode control to the user setting.
    m_pConfig->setValue(keepQueueKey, enabled);
    // Kept in lockstep here so skins never see the two disagree.
    m_keepQueueOff.set(enabled ? 0.0 : 1.0);
    // Leaving Tango mode has to clear the deck warning with everything else.
    updatePauseAfterDeckControl();
}

void AutoDJProcessor::controlKeepQueueChangeRequest(double value) {
    const bool enabled = value > 0.0;
    // TangoQ locks Tango DJ mode on for the running application (see
    // lockTangoModeOn()). Once locked it can never be turned off: confirm it back
    // on for any disable request, e.g. a stray controller binding. The unit tests
    // leave it unlocked so the stock (non-Tango) Auto DJ path stays exercised,
    // which is what keeps a revert of this commit safe. Reverting this commit
    // drops the lock and restores the user-switchable behaviour below.
    if (m_tangoModeLocked && !enabled) {
        m_keepQueue.setAndConfirm(1.0);
        m_keepQueueOff.set(0.0);
        return;
    }
    // Tango mode switches the Auto DJ queue between two incompatible behaviours
    // (cursor-based versus consume-from-the-top), so it may only change while
    // Auto DJ is stopped - the same rule the preferences checkbox enforces by
    // disabling itself. Refuse otherwise by confirming the current value, which
    // also snaps any toggle that requested the change back to reality.
    if (m_eState != ADJ_DISABLED) {
        qDebug() << "Tango mode can only be changed while Auto DJ is stopped";
        m_keepQueue.setAndConfirm(m_keepQueue.get());
        return;
    }
    resetAllAutoDJFadeGains();
    stopTandaCrossfaderAnimation();
    if (enabled) {
        setTransitionMode(TransitionMode::TandaTransition);
    } else if (isTandaTransition(m_transitionMode)) {
        restoreLastStockTransitionMode();
    }
    m_keepQueue.setAndConfirm(enabled ? 1.0 : 0.0);
    // The mirror has to be updated here, not only in controlKeepQueue(): a
    // ControlObject does not emit valueChanged for a change it made itself, so
    // setAndConfirm() above never reaches that slot. Every route into Tango mode
    // - keyboard shortcut, controller, preferences checkbox - arrives as a change
    // *request* and lands here, so this is the path that actually runs.
    m_keepQueueOff.set(enabled ? 0.0 : 1.0);
    m_pConfig->setValue(ConfigKey(kPreferenceGroup, QStringLiteral("KeepQueue")), enabled);
}

void AutoDJProcessor::lockTangoModeOn() {
    // TangoQ ships tango-only: lock Tango DJ mode on for good. Setting the control
    // routes through controlKeepQueueChangeRequest() above, which selects the
    // Tanda transition and updates the keep_queue_off mirror; the lock then
    // refuses every later attempt to turn it off. Only the running application
    // calls this - the unit tests never do, so they can still toggle keep_queue to
    // verify the stock Auto DJ path. Reverting the commit that added this call
    // restores the user-switchable Tango mode.
    m_tangoModeLocked = true;
    m_keepQueue.set(1.0);
}

void AutoDJProcessor::controlCortinaLength(double value) {
    // Clamp to the same range as the Preferences field. The UI setters clamp
    // before writing, so this only guards against out-of-range values from
    // elsewhere; don't write the control back to avoid a feedback loop.
    int seconds = static_cast<int>(std::lround(value));
    seconds = std::clamp(seconds, 5, 600);
    m_pConfig->setValue(
            ConfigKey(kPreferenceGroup, QStringLiteral("CortinaLength")), seconds);
    if (seconds != m_keepQueueCortinaSeconds) {
        m_keepQueueCortinaSeconds = seconds;
        // The budget feeds the cached set-length sums, so force a recompute.
        m_keepQueueDurationDirty = true;
    }
}

void AutoDJProcessor::controlResetQueueState(double value) {
    if (value <= 0.0) {
        return;
    }
    resetKeepQueueSet();
    // Re-arm the momentary trigger so the next request fires even though the
    // control value was already 1 (ControlObject only emits on a change).
    m_resetQueueState.set(0.0);
}

void AutoDJProcessor::resetKeepQueueSet() {
    // Guard: this only makes sense for a stopped Keep Queue (Tango) set. The menu
    // action is already gated the same way; this is defence in depth against the
    // control being triggered otherwise.
    if (!keepQueueEnabled() || m_eState != ADJ_DISABLED) {
        return;
    }
    const int rowCount = m_pAutoDJTableModel->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        TrackPointer pTrack = m_pAutoDJTableModel->getTrack(
                m_pAutoDJTableModel->index(row, 0));
        if (pTrack) {
            // Clear the played status (and its grey colour) but keep the user's
            // real play counts. The model recolours the row via its tracksChanged
            // -> dataChanged path.
            pTrack->updatePlayedStatusKeepPlayCount(false);
        }
    }
    // Restart from the top: cursor to the first track, anchor cleared.
    m_keepQueueRow = 0;
    m_keepQueueAnchorId = TrackId();
    invalidateRemainingSetDuration();
}

bool AutoDJProcessor::advanceKeepQueueCursor(TrackPointer pTrack) {
    if (!pTrack) {
        return false;
    }
    TrackId trackId(pTrack->getId());
    if (!trackId.isValid()) {
        return false;
    }
    // Only advance if the played track is the current cursor track. This guards
    // against advancing twice when this is called more than once for the same
    // track (mirrors the guard in removeTrackFromTopOfQueue).
    TrackId cursorId(m_pAutoDJTableModel->getTrackId(
            m_pAutoDJTableModel->index(m_keepQueueRow, 0)));
    if (!cursorId.isValid() || trackId != cursorId) {
        return false;
    }
    m_keepQueueRow++;
    updatePauseAfterDeckControl();
    // The upcoming-tracks set shrank by one, so the cached set duration is stale.
    invalidateRemainingSetDuration();
    // Remember the just-played track; it now sits at cursor-1 and is used to
    // re-anchor the cursor across model rebuilds while Auto DJ is stopped.
    m_keepQueueAnchorId = trackId;
    return true;
}

void AutoDJProcessor::reanchorKeepQueueCursor() {
    if (!keepQueueEnabled()) {
        return;
    }
    const int rowCount = m_pAutoDJTableModel->rowCount();
    // The model is momentarily empty in the middle of every select() rebuild (a
    // transient clear followed by a full insert). Do NOT reset the cursor here, or
    // an edit made while Auto DJ is stopped would stick at 0; the cursor is
    // re-anchored on the following insert pass.
    if (rowCount == 0) {
        return;
    }
    if (m_eState == ADJ_DISABLED) {
        // Stopped: there is no playing deck to anchor to. Re-anchor the cursor to
        // the last-played track by identity so it survives the rebuild and we
        // continue from the next unplayed track instead of jumping to the top.
        // If that track is gone (the queue was cleared), restart from the top.
        if (m_keepQueueAnchorId.isValid()) {
            const int anchorRow =
                    keepQueueRowForTrackId(m_keepQueueAnchorId, m_keepQueueRow - 1);
            m_keepQueueRow = (anchorRow >= 0) ? anchorRow + 1 : 0;
        } else {
            m_keepQueueRow = 0;
        }
        if (m_keepQueueRow > rowCount) {
            m_keepQueueRow = rowCount;
        }
        return;
    }
    DeckAttributes* pFromDeck = getFromDeck();
    if (pFromDeck) {
        // Primary anchor: the playing track sits just before the cursor.
        const int playingRow = keepQueueRowForTrack(
                pFromDeck->getLoadedTrack(), m_keepQueueRow - 1);
        if (playingRow >= 0) {
            m_keepQueueRow = playingRow + 1;
        } else {
            // The playing track was removed from the queue. Fall back to the
            // cued track on the idle deck, which sits at the cursor.
            DeckAttributes* pIdleDeck = getOtherDeck(pFromDeck);
            if (pIdleDeck && !pIdleDeck->isPlaying()) {
                const int cuedRow = keepQueueRowForTrack(
                        pIdleDeck->getLoadedTrack(), m_keepQueueRow);
                if (cuedRow >= 0) {
                    m_keepQueueRow = cuedRow;
                }
            }
        }
    }
    // Keep the cursor within range if the queue shrank and no anchor was found.
    if (m_keepQueueRow > m_pAutoDJTableModel->rowCount()) {
        m_keepQueueRow = m_pAutoDJTableModel->rowCount();
    }

    // A queue edit may have changed which track is next (e.g. the cued track was
    // deleted or reordered). Reload the idle deck so it isn't left holding a
    // stale track. Guard against re-entrancy from the reload mutating the queue.
    if (!m_keepQueueReloading) {
        m_keepQueueReloading = true;
        maybeReloadIdleDeckForKeepQueue();
        m_keepQueueReloading = false;
    }
}

int AutoDJProcessor::keepQueueRowForTrack(TrackPointer pTrack, int rowGuess) {
    if (!pTrack) {
        return -1;
    }
    return keepQueueRowForTrackId(TrackId(pTrack->getId()), rowGuess);
}

int AutoDJProcessor::keepQueueRowForTrackId(TrackId trackId, int rowGuess) {
    if (!trackId.isValid()) {
        return -1;
    }
    const QVector<int> rows = m_pAutoDJTableModel->getTrackRows(trackId);
    if (rows.isEmpty()) {
        return -1;
    }
    // If the track occurs more than once, pick the occurrence closest to the
    // guess to disambiguate.
    int bestRow = rows.first();
    for (int row : rows) {
        if (qAbs(row - rowGuess) < qAbs(bestRow - rowGuess)) {
            bestRow = row;
        }
    }
    return bestRow;
}

void AutoDJProcessor::maybeReloadIdleDeckForKeepQueue() {
    if (m_eState != ADJ_IDLE) {
        return;
    }
    DeckAttributes* pFromDeck = getFromDeck();
    if (!pFromDeck) {
        // Running out of queue neutralises the pending transition, which leaves
        // no deck flagged as the "from" deck. The playing deck still is one in
        // every sense that matters here, so fall back to it - this is the path
        // that picks a set back up when the next tanda is appended mid-set.
        for (const auto& pDeck : m_decks) {
            if (pDeck && pDeck->isPlaying()) {
                pFromDeck = pDeck.get();
                break;
            }
        }
    }
    if (!pFromDeck) {
        return;
    }
    DeckAttributes* pIdleDeck = getOtherDeck(pFromDeck);
    if (!pIdleDeck || pIdleDeck->isPlaying()) {
        return;
    }
    // The next track to play is at the cursor row.
    TrackPointer pNextTrack = m_pAutoDJTableModel->getTrack(
            m_pAutoDJTableModel->index(m_keepQueueRow, 0));
    // Only reload when there is a valid next track that differs from the one
    // already cued on the idle deck (e.g. the cued track was deleted/reordered).
    if (pNextTrack && pNextTrack != pIdleDeck->getLoadedTrack()) {
        loadNextTrackFromQueue(*pIdleDeck);
    }
}

void AutoDJProcessor::maybeFillRandomTracks() {
    int minAutoDJCrateTracks =
            m_pConfig->getValueString(ConfigKey(kPreferenceGroup,
                                              QStringLiteral("RandomQueueMinimumAllowed")))
                    .toInt();
    bool randomQueueEnabled =
            m_pConfig->getValueString(
                             ConfigKey(kPreferenceGroup,
                                     QStringLiteral("EnableRandomQueue")))
                    .toInt() == 1;

    int tracksToAdd = minAutoDJCrateTracks - m_pAutoDJTableModel->rowCount();
    if (randomQueueEnabled && (tracksToAdd > 0)) {
        qDebug() << "Randomly adding tracks";
        emit randomTrackRequested(tracksToAdd);
    }
}

void AutoDJProcessor::playerPlayChanged(DeckAttributes* thisDeck, bool playing) {
    if constexpr (sDebug) {
        qDebug() << this << "playerPlayChanged" << thisDeck->group << playing;
    }

    if (isTandaGapPending()) {
        return;
    }

    // The queue ran dry earlier and we stayed enabled to let the last track
    // finish. Once nothing is playing any more the set really is over. Checked
    // ahead of the state test below so a deferred stop can never be stranded.
    // Exception: a cortina's before-gap stops its deck on purpose and resumes it
    // when the gap elapses, so that pause is not the end of the set. Stopping
    // here would cancel the fade and strand the cortina in its silent lead-in.
    // The after-gap is not excluded: with nothing left to hand off to, ending
    // there is right and drops the now-pointless gap.
    if (!playing && thisDeck->playPosition() >= 1.0 && automaticStopPending() &&
            m_cortinaFadePhase != CortinaFadePhase::BeforeGap &&
            !anyDeckPlaying()) {
        // Left set deliberately: toggleAutoDJ() reads it to tell this automatic
        // stop from a manual one (so the LIVE stop-guard stays out of the way)
        // and clears it itself.
        toggleAutoDJ(false);
        return;
    }

    if (m_eState != ADJ_IDLE) {
        // We don't want to recalculate a running transition
        return;
    }

    if (thisDeck->loading) {
        // Note: When loading a new deck this signal arrives before the
        // playerTrackLoaded();
        return;
    }

    DeckAttributes* otherDeck = getOtherDeck(thisDeck);
    if (!otherDeck) {
        // This happens if all decks have center orientation
        return;
    }

    if (playing) {
        if (!otherDeck->isPlaying()) {
            // In case both decks were stopped and now this one just started, make
            // this deck the "from deck".
            calculateTransition(thisDeck, getOtherDeck(thisDeck), false);
        }
    } else {
        // Deck paused
        // This may happen if the user has previously pressed play on the "to deck"
        // before fading, for example to adjust the intro/outro cues, and lets the
        // deck play until the end, seek back to the start point instead of keeping
        if (thisDeck->playPosition() >= 1.0 && !thisDeck->isFromDeck) {
            // toDeck has stopped at the end. Recalculate the transition, because
            // it has been done from a now irrelevant previous position.
            // This forces the other deck to be the fromDeck.
            thisDeck->startPos = kKeepPosition;
            calculateTransition(otherDeck, thisDeck, true);
            if (thisDeck->startPos != kKeepPosition) {
                // Note: this seek will trigger the playerPositionChanged slot
                // which may calls the calculateTransition() again without seek = true;
                thisDeck->setPlayPosition(thisDeck->startPos);
            }
        }
    }
}

void AutoDJProcessor::playerIntroStartChanged(DeckAttributes* pAttributes, double position) {
    if constexpr (sDebug) {
        qDebug() << this << "playerIntroStartChanged" << pAttributes->group << position;
    }
    // nothing to do, because we want not to re-cue the toDeck and the from
    // Deck has already passed the intro
}

void AutoDJProcessor::playerIntroEndChanged(DeckAttributes* pAttributes, double position) {
    if constexpr (sDebug) {
        qDebug() << this << "playerIntroEndChanged" << pAttributes->group << position;
    }

    if (m_eState != ADJ_IDLE) {
        // We don't want to recalculate a running transition
        return;
    }

    if (pAttributes->isFromDeck) {
        // We have already passed the intro
        return;
    }
    DeckAttributes* fromDeck = getFromDeck();
    if (!fromDeck) {
        return;
    }
    calculateTransition(fromDeck, getOtherDeck(fromDeck), false);
}

void AutoDJProcessor::playerOutroStartChanged(DeckAttributes* pAttributes, double position) {
    if constexpr (sDebug) {
        qDebug() << this << "playerOutroStartChanged" << pAttributes->group << position;
    }

    if (m_eState != ADJ_IDLE) {
        // We don't want to recalculate a running transition
        return;
    }

    DeckAttributes* fromDeck = getFromDeck();
    if (!fromDeck) {
        return;
    }
    calculateTransition(fromDeck, getOtherDeck(fromDeck), false);
}

void AutoDJProcessor::playerOutroEndChanged(DeckAttributes* pAttributes, double position) {
    if constexpr (sDebug) {
        qDebug() << this << "playerOutroEndChanged" << pAttributes->group << position;
    }

    if (m_eState != ADJ_IDLE) {
        // We don't want to recalculate a running transition
        return;
    }

    DeckAttributes* fromDeck = getFromDeck();
    if (!fromDeck) {
        return;
    }
    calculateTransition(fromDeck, getOtherDeck(fromDeck), false);
}

double AutoDJProcessor::getIntroStartSecond(DeckAttributes* pDeck) {
    if (m_keepQueue.toBool()) {
        const TrackPointer pTrack = pDeck ? pDeck->getLoadedTrack() : TrackPointer();
        if (pTrack) {
            const CuePointer pIntroCue = pTrack->findCueByType(mixxx::CueType::Intro);
            const CuePointer pFasCue = pTrack->findCueByType(mixxx::CueType::N60dBSound);
            const auto classification = mixxx::tango::classifyStartCue(
                    pIntroCue ? pIntroCue->getPosition() : mixxx::audio::FramePos(),
                    pIntroCue ? pIntroCue->getLabel() : QString(),
                    pFasCue ? pFasCue->getPosition() : mixxx::audio::FramePos());
            if (!classification.hasExplicitStart() && !classification.hasFas() &&
                    pDeck->introStartPosition().isValid()) {
                return framePositionToSeconds(pDeck->introStartPosition(), pDeck);
            }
            const auto startPosition = mixxx::tango::tangoPlaybackStart(classification);
            if (startPosition.isValid()) {
                return framePositionToSeconds(startPosition, pDeck);
            }
        }
        // Some deck integrations expose controls without retaining a Track
        // pointer. Preserve their existing start signal as a last-resort
        // fallback; loaded tracks always take the classified path above.
        if (pDeck && pDeck->introStartPosition().isValid()) {
            return framePositionToSeconds(pDeck->introStartPosition(), pDeck);
        }
        return 0.0;
    }
    const mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();
    const mixxx::audio::FramePos introStartPosition = pDeck->introStartPosition();
    const mixxx::audio::FramePos introEndPosition = pDeck->introEndPosition();
    if (!introStartPosition.isValid() || introStartPosition > trackEndPosition) {
        double firstSoundSecond = getFirstSoundSecond(pDeck);
        if (!introEndPosition.isValid() || introEndPosition > trackEndPosition) {
            // No intro start and intro end set, use First Sound.
            return firstSoundSecond;
        }
        double introEndSecond = framePositionToSeconds(introEndPosition, pDeck);
        if (m_transitionTime >= 0) {
            return introEndSecond - m_transitionTime;
        }
        return introEndSecond;
    }
    return framePositionToSeconds(introStartPosition, pDeck);
}

double AutoDJProcessor::getIntroEndSecond(DeckAttributes* pDeck) {
    const mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();
    const mixxx::audio::FramePos introEndPosition = pDeck->introEndPosition();
    if (!introEndPosition.isValid() || introEndPosition > trackEndPosition) {
        // Assume a zero length intro if introEnd is not set.
        // The introStart is automatically placed by AnalyzerSilence, so use
        // that as a fallback if the user has not placed outroStart. If it has
        // not been placed, getIntroStartPosition will return 0:00.
        return getIntroStartSecond(pDeck);
    }
    return framePositionToSeconds(introEndPosition, pDeck);
}

double AutoDJProcessor::getOutroStartSecond(DeckAttributes* pDeck) {
    const mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();
    const mixxx::audio::FramePos outroStartPosition = pDeck->outroStartPosition();
    if (!outroStartPosition.isValid() || outroStartPosition > trackEndPosition) {
        // Assume a zero length outro if outroStart is not set.
        // The outroEnd is automatically placed by AnalyzerSilence, so use
        // that as a fallback if the user has not placed outroStart. If it has
        // not been placed, getOutroEndPosition will return the end of the track.
        return getOutroEndSecond(pDeck);
    }
    return framePositionToSeconds(outroStartPosition, pDeck);
}

double AutoDJProcessor::getOutroEndSecond(DeckAttributes* pDeck) {
    const mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();
    const mixxx::audio::FramePos outroStartPosition = pDeck->outroStartPosition();
    const mixxx::audio::FramePos outroEndPosition = pDeck->outroEndPosition();
    if (!outroEndPosition.isValid() || outroEndPosition > trackEndPosition) {
        double lastSoundSecond = getLastSoundSecond(pDeck);
        DEBUG_ASSERT(lastSoundSecond <= framePositionToSeconds(trackEndPosition, pDeck));
        if (!outroStartPosition.isValid() || outroStartPosition > trackEndPosition) {
            // No outro start and outro end set, use Last Sound.
            return lastSoundSecond;
        }
        // Try to find a better Outro End using Outro Start and transition time
        double outroStartSecond = framePositionToSeconds(outroStartPosition, pDeck);
        if (m_transitionTime >= 0 && lastSoundSecond > outroStartSecond) {
            double outroEndFromTime = outroStartSecond + m_transitionTime;
            if (outroEndFromTime < lastSoundSecond) {
                // The outroEnd is automatically placed by AnalyzerSilence at the last sound
                // Here the user has removed it, but has placed a outro start.
                // Use the transition time instead of the dismissed last sound position.
                return outroEndFromTime;
            }
            return lastSoundSecond;
        }
        return outroStartSecond;
    }
    return framePositionToSeconds(outroEndPosition, pDeck);
}

double AutoDJProcessor::getFirstSoundSecond(DeckAttributes* pDeck) {
    TrackPointer pTrack = pDeck->getLoadedTrack();
    if (!pTrack) {
        return 0.0;
    }

    CuePointer pFromTrackN60dBSound = pTrack->findCueByType(mixxx::CueType::N60dBSound);
    if (pFromTrackN60dBSound) {
        const mixxx::audio::FramePos firstSound = pFromTrackN60dBSound->getPosition();
        if (firstSound.isValid()) {
            const mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();
            if (firstSound <= trackEndPosition) {
                return framePositionToSeconds(firstSound, pDeck);
            } else {
                qWarning() << "-60 dB Sound Cue starts after track end in:"
                           << pTrack->getLocation()
                           << "Using the first sample instead.";
            }
        }
    }
    return 0.0;
}

double AutoDJProcessor::getLastSoundSecond(DeckAttributes* pDeck) {
    TrackPointer pTrack = pDeck->getLoadedTrack();
    if (!pTrack) {
        return 0.0;
    }

    const mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();
    CuePointer pFromTrackN60dBSound = pTrack->findCueByType(mixxx::CueType::N60dBSound);
    if (pFromTrackN60dBSound && pFromTrackN60dBSound->getLengthFrames() > 0.0) {
        const mixxx::audio::FramePos lastSound = pFromTrackN60dBSound->getEndPosition();
        if (lastSound > mixxx::audio::FramePos(0.0)) {
            if (lastSound <= trackEndPosition) {
                return framePositionToSeconds(lastSound, pDeck);
            } else {
                qWarning() << "-60 dB Sound Cue ends after track end in:"
                           << pTrack->getLocation()
                           << "Using the last sample instead.";
            }
        }
    }
    return framePositionToSeconds(trackEndPosition, pDeck);
}

double AutoDJProcessor::getEndSecond(DeckAttributes* pDeck) {
    TrackPointer pTrack = pDeck->getLoadedTrack();
    if (!pTrack) {
        return 0.0;
    }

    mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();
    return framePositionToSeconds(trackEndPosition, pDeck);
}

double AutoDJProcessor::framePositionToSeconds(
        mixxx::audio::FramePos position, DeckAttributes* pDeck) {
    mixxx::audio::SampleRate sampleRate = pDeck->sampleRate();
    if (!sampleRate.isValid() || !position.isValid()) {
        return 0.0;
    }

    return position.value() / sampleRate / pDeck->rateRatio();
}

void AutoDJProcessor::calculateTransition(DeckAttributes* pFromDeck,
        DeckAttributes* pToDeck,
        bool seekToStartPoint) {
    VERIFY_OR_DEBUG_ASSERT(pFromDeck && pToDeck) {
        return;
    }
    if (pFromDeck->loading || pToDeck->loading) {
        // don't use halve new halve old data during
        // changing of tracks
        return;
    }

    // We require ADJ_IDLE to prevent changing the thresholds in the middle of a
    // fade.
    VERIFY_OR_DEBUG_ASSERT(m_eState == ADJ_IDLE) {
        return;
    }

    const double fromDeckEndPosition = getEndSecond(pFromDeck);
    const double toDeckEndPosition = getEndSecond(pToDeck);
    // Since the end position is measured in seconds from 0:00 it is also
    // the track duration. Use this alias for better readability.
    const double fromDeckDuration = fromDeckEndPosition;
    const double toDeckDuration = toDeckEndPosition;

    VERIFY_OR_DEBUG_ASSERT(fromDeckDuration >= kMinimumTrackDurationSec) {
        // Track has no duration or too short. This should not happen, because short
        // tracks are skipped after load. Play ToDeck immediately.
        pFromDeck->fadeBeginPos = 0;
        pFromDeck->fadeEndPos = 0;
        pToDeck->startPos = kKeepPosition;
        return;
    }
    if (toDeckDuration == 0) {
        // This is a seek call to zero after ejecting the track
        // this signal is received before the track pointer becomes null
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(toDeckDuration >= kMinimumTrackDurationSec) {
        // Track has no duration or too short. This should not happen, because short
        // tracks are skipped after load.
        loadNextTrackFromQueue(*pToDeck, false);
        return;
    }

    // Within this function, the outro refers to the outro of the currently
    // playing track and the intro refers to the intro of the next track.

    double outroEnd = getOutroEndSecond(pFromDeck);
    double outroStart = getOutroStartSecond(pFromDeck);
    const double fromDeckPosition = fromDeckDuration * pFromDeck->playPosition();

    VERIFY_OR_DEBUG_ASSERT(outroEnd <= fromDeckEndPosition) {
        outroEnd = fromDeckEndPosition;
    }

    if (fromDeckPosition > outroStart) {
        // We have already passed outroStart
        // This can happen if we have just enabled auto DJ
        outroStart = fromDeckPosition;
        if (fromDeckPosition > outroEnd) {
            outroEnd = math_min(outroStart + fabs(m_transitionTime), fromDeckEndPosition);
        }
    }
    double outroLength = outroEnd - outroStart;

    double toDeckPositionSeconds = toDeckDuration * pToDeck->playPosition();
    // Store here a possible fadeBeginPos for the transition after next
    // This is used to check if it will be possible or a re-cue is required.
    // here it is done for FullIntroOutro and FadeAtOutroStart.
    // It is adjusted below for the other modes.
    pToDeck->fadeEndPos = getOutroEndSecond(pToDeck);
    double toDeckOutroStartSecond = getOutroStartSecond(pToDeck);
    if (pToDeck->fadeEndPos == toDeckOutroStartSecond) {
        // outro not defined, use transition time.
        toDeckOutroStartSecond -= m_transitionTime;
    }
    pToDeck->fadeBeginPos = toDeckOutroStartSecond;

    double toDeckStartSeconds = toDeckPositionSeconds;
    const double introStart = getIntroStartSecond(pToDeck);
    const double introEnd = getIntroEndSecond(pToDeck);
    if (seekToStartPoint || toDeckPositionSeconds >= pToDeck->fadeBeginPos) {
        // toDeckPosition >= pToDeck->fadeBeginPos happens when the
        // user has seeked or played the to track behind fadeBeginPos of
        // the fade after the next.
        // In this case we recue the track just before the transition.
        toDeckStartSeconds = introStart;
    }

    const double toIntroCueSecond = framePositionToSeconds(pToDeck->introStartPosition(), pToDeck);
    TT_TRACE() << "calculate boundary from=" << pFromDeck->group
               << " to=" << pToDeck->group
               << " mode=" << transitionModeName(m_transitionMode)
               << " keepQueue=" << yesNo(keepQueueEnabled())
               << " cortinaFade=" << yesNo(m_cortinaFadeEnabled)
               << " fromCortina=" << yesNo(isCortina(pFromDeck->getLoadedTrack()))
               << " toCortina=" << yesNo(isCortina(pToDeck->getLoadedTrack()))
               << " seekToStartPoint=" << yesNo(seekToStartPoint)
               << " transitionTime=" << m_transitionTime
               << " cortinaGap=" << m_cortinaGapSeconds
               << " fromLastSound=" << getLastSoundSecond(pFromDeck)
               << " toFirstSound=" << getFirstSoundSecond(pToDeck)
               << " toIntroCue=" << toIntroCueSecond;

    double introLength = 0;

    // introEnd is equal introStart in case it has not yet been set
    if (toDeckStartSeconds < introEnd && introStart < introEnd) {
        // Limit the intro length that results from a revers seek
        // to a reasonable values. If the seek was too big, ignore it.
        introLength = introEnd - toDeckStartSeconds;
        if (introLength > (introEnd - introStart) * 2 &&
                introLength > (introEnd - introStart) + m_transitionTime &&
                introLength > outroLength) {
            introLength = 0;
        }
    }

    if constexpr (sDebug) {
        qDebug() << this << "calculateTransition"
                 << "introLength" << introLength
                 << "outroLength" << outroLength;
    }

    switch (m_transitionMode) {
    case TransitionMode::FullIntroOutro: {
        // Use the outro or intro length for the transition time, whichever is
        // shorter. Let the full outro and intro play; do not cut off any part
        // of either.
        //
        // In the diagrams below,
        // - is part of a track outside the outro/intro,
        // o is part of the outro
        // i is part of the intro
        // | marks the boundaries of the transition
        //
        // When outro > intro:
        // ------ooo|ooo|
        //          |iii|------
        //
        // When outro < intro:
        // ------|ooo|
        //       |iii|iii-----
        //
        // If only the outro or intro length is marked but not both, use the one
        // that is marked for the transition time. If neither is marked, fall
        // back to the transition time from the spinbox.
        double transitionLength = introLength;
        if (outroLength > 0) {
            if (transitionLength <= 0 || transitionLength > outroLength) {
                // Use outro length when the intro is not defined or longer
                // than the outro.
                transitionLength = outroLength;
            }
        }
        if (transitionLength > 0) {
            const double transitionEnd = toDeckStartSeconds + transitionLength;
            if (transitionEnd > pToDeck->fadeBeginPos) {
                // End intro before next outro starts
                transitionLength = pToDeck->fadeBeginPos - toDeckStartSeconds;
                VERIFY_OR_DEBUG_ASSERT(transitionLength > 0) {
                    // We seek to intro start above in this case so this never happens
                    transitionLength = 1;
                }
            }
            pFromDeck->fadeBeginPos = outroEnd - transitionLength;
            pFromDeck->fadeEndPos = outroEnd;
            pToDeck->startPos = toDeckStartSeconds;
        } else {
            useFixedFadeTime(pFromDeck, pToDeck, fromDeckPosition, outroEnd, toDeckStartSeconds);
        }
    } break;
    case TransitionMode::FadeAtOutroStart: {
        // Use the outro or intro length for the transition time, whichever is
        // shorter. If the outro is longer than the intro, cut off the end
        // of the outro.
        //
        // In the diagrams below,
        // - is part of a track outside the outro/intro,
        // o is part of the outro
        // i is part of the intro
        // | marks the boundaries of the transition
        //
        // When outro > intro:
        // ------|ooo|ooo
        //       |iii|------
        //
        // When outro < intro:
        // ------|ooo|
        //       |iii|iii-----
        //
        // If only the outro or intro length is marked but not both, use the one
        // that is marked for the transition time. If neither is marked, fall
        // back to the transition time from the spinbox.
        double transitionLength = outroLength;
        if (transitionLength > 0) {
            if (introLength > 0) {
                if (outroLength > introLength) {
                    // Cut off end of outro
                    transitionLength = introLength;
                }
            }
            const double transitionEnd = toDeckStartSeconds + transitionLength;
            if (transitionEnd > pToDeck->fadeBeginPos) {
                // End intro before next outro starts
                transitionLength = pToDeck->fadeBeginPos - toDeckStartSeconds;
                VERIFY_OR_DEBUG_ASSERT(transitionLength > 0) {
                    // We seek to intro start above in this case so this never happens
                    transitionLength = 1;
                }
            }
            pFromDeck->fadeBeginPos = outroStart;
            pFromDeck->fadeEndPos = outroStart + transitionLength;
            pToDeck->startPos = toDeckStartSeconds;
        } else if (introLength > 0) {
            transitionLength = introLength;
            pFromDeck->fadeBeginPos = outroEnd - transitionLength;
            pFromDeck->fadeEndPos = outroEnd;
            pToDeck->startPos = toDeckStartSeconds;
        } else {
            useFixedFadeTime(pFromDeck, pToDeck, fromDeckPosition, outroEnd, toDeckStartSeconds);
        }
    } break;
    case TransitionMode::TandaTransition: {
        const double toDeckStartSecond = tandaEntryPointSecond(pToDeck);
        pFromDeck->fadeBeginPos = getLastSoundSecond(pFromDeck);
        pFromDeck->fadeEndPos = pFromDeck->fadeBeginPos;
        pToDeck->startPos = toDeckStartSecond;
        pToDeck->fadeBeginPos = getLastSoundSecond(pToDeck);
        TT_TRACE() << "tanda calculate toStartSecond=" << toDeckStartSecond
                   << " fromLastSound=" << pFromDeck->fadeBeginPos;
    } break;
    case TransitionMode::FixedSkipSilence: {
        double toDeckStartSecond;
        pToDeck->fadeBeginPos = getLastSoundSecond(pToDeck);
        if (seekToStartPoint || toDeckPositionSeconds >= pToDeck->fadeBeginPos) {
            // toDeckPosition >= pToDeck->fadeBeginPos happens when the
            // user has seeked or played the to track behind fadeBeginPos of
            // the fade after the next.
            // In this case we recue the track just before the transition.
            toDeckStartSecond = getFirstSoundSecond(pToDeck);
            TT_TRACE() << "fixedSkipSilence recue toStartSecond=" << toDeckStartSecond;
        } else {
            toDeckStartSecond = toDeckPositionSeconds;
            TT_TRACE() << "fixedSkipSilence keepPosition toStartSecond=" << toDeckStartSecond;
        }
        useFixedFadeTime(
                pFromDeck,
                pToDeck,
                fromDeckPosition,
                getLastSoundSecond(pFromDeck),
                toDeckStartSecond);
    } break;
    case TransitionMode::FixedFullTrack:
    default: {
        double startPoint;
        pToDeck->fadeBeginPos = toDeckEndPosition;
        if (seekToStartPoint || toDeckPositionSeconds >= pToDeck->fadeBeginPos) {
            // toDeckPosition >= pToDeck->fadeBeginPos happens when the
            // user has seeked or played the to track behind fadeBeginPos of
            // the fade after the next.
            // In this case we recue the track just before the transition.
            startPoint = 0.0;
        } else {
            startPoint = toDeckPositionSeconds;
        }
        useFixedFadeTime(pFromDeck, pToDeck, fromDeckPosition, fromDeckEndPosition, startPoint);
        }
    }

    // Automated cortina fade: a D -> cortina boundary is always a hard cut
    // followed by an Nc-second silent gap (the cortina handler then ramps the
    // crossfader in), independent of the intra-tanda transition time Nt. Force
    // the hard cut and cue the cortina Nc seconds ahead of its first audible
    // sample: the hard cut then lands inside the cortina's silent lead-in, so
    // the callbacks until maybeHandleCortinaFade() pauses it are inaudible.
    // (The true Nc gap itself is held wall-clock by the gap timer.)
    //
    // Deliberately NOT clamped to >= 0: when the cortina has less than Nc of
    // real leading silence (a hot start, or an un-analyzed cortina whose
    // first-sound cue defaults to 0:00), a negative start position makes the
    // engine pre-roll synthetic silence up to 0:00 - the same mechanism a
    // negative transition time uses. That guarantees the hard cut always lands
    // in silence, so the cortina's onset never reaches the output at full
    // crossfader (which is the "pop on hot-start cortinas" this avoids).
    if (m_transitionMode != TransitionMode::TandaTransition &&
            m_cortinaFadeEnabled && keepQueueEnabled() &&
            isCortina(pToDeck->getLoadedTrack())) {
        TT_TRACE() << "cortina override incoming fromLastSound=" << getLastSoundSecond(pFromDeck)
                   << " toFirstSound=" << getFirstSoundSecond(pToDeck)
                   << " gap=" << m_cortinaGapSeconds;
        pFromDeck->fadeBeginPos = getLastSoundSecond(pFromDeck);
        pFromDeck->fadeEndPos = pFromDeck->fadeBeginPos;
        pToDeck->startPos = getFirstSoundSecond(pToDeck) -
                static_cast<double>(m_cortinaGapSeconds);
    }

    TT_TRACE() << "calculate result-seconds fromFadeBegin=" << pFromDeck->fadeBeginPos
               << " fromFadeEnd=" << pFromDeck->fadeEndPos
               << " toStart=" << pToDeck->startPos
               << " toFadeBegin=" << pToDeck->fadeBeginPos
               << " toFadeEnd=" << pToDeck->fadeEndPos;

    // These are expected to be a fraction of the track length.
    pFromDeck->fadeBeginPos /= fromDeckDuration;
    pFromDeck->fadeEndPos /= fromDeckDuration;
    pToDeck->startPos /= toDeckDuration;
    pToDeck->fadeBeginPos /= toDeckDuration;
    pToDeck->fadeEndPos /= toDeckDuration;

    pFromDeck->isFromDeck = true;
    pToDeck->isFromDeck = false;

    VERIFY_OR_DEBUG_ASSERT(pFromDeck->fadeBeginPos <= 1) {
        pFromDeck->fadeBeginPos = 1;
    }

    if constexpr (sDebug) {
        qDebug() << this << "calculateTransition" << pFromDeck->group
                 << pFromDeck->fadeBeginPos << pFromDeck->fadeEndPos
                 << pToDeck->startPos;
    }
}

double AutoDJProcessor::tandaEntryPointSecond(DeckAttributes* pDeck) {
    if (!pDeck) {
        return 0.0;
    }
    if (m_keepQueue.toBool()) {
        return getIntroStartSecond(pDeck);
    }
    const mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();
    const mixxx::audio::FramePos introStartPosition = pDeck->introStartPosition();
    if (introStartPosition.isValid() && introStartPosition <= trackEndPosition) {
        return framePositionToSeconds(introStartPosition, pDeck);
    }
    return getFirstSoundSecond(pDeck);
}

int AutoDJProcessor::tandaGapSecondsFor(
        DeckAttributes* pFromDeck, DeckAttributes* pToDeck) const {
    Q_UNUSED(pFromDeck);
    Q_UNUSED(pToDeck);
    // Keep the seam split even while the product exposes one value. If feedback
    // asks for separate intra-tanda and cortina-boundary gaps, this becomes the
    // switch point instead of spreading that decision through transition code.
    return m_tandaGapSeconds;
}

bool AutoDJProcessor::shouldUseTandaGap(
        DeckAttributes* pFromDeck, DeckAttributes* pToDeck) const {
    return keepQueueEnabled() &&
            m_transitionMode == TransitionMode::TandaTransition &&
            pFromDeck &&
            pToDeck &&
            !pToDeck->loading;
}

bool AutoDJProcessor::isTandaGapPending() const {
    return m_tandaGapTimer.isActive() || m_pTandaFromDeck || m_pTandaToDeck;
}

void AutoDJProcessor::startTandaGap(DeckAttributes* pFromDeck, DeckAttributes* pToDeck) {
    if (!shouldUseTandaGap(pFromDeck, pToDeck)) {
        return;
    }
    const TrackPointer pToTrack = pToDeck->getLoadedTrack();
    const double toDuration = getEndSecond(pToDeck);
    if (!pToTrack || toDuration < kMinimumTrackDurationSec) {
        toggleAutoDJ(false);
        return;
    }

    m_pTandaFromDeck = pFromDeck;
    m_pTandaToDeck = pToDeck;
    m_tandaToTrackId = TrackId(pToTrack->getId());
    m_tandaEntrySecond = tandaEntryPointSecond(pToDeck);

    TT_TRACE() << "tanda gap start from=" << pFromDeck->group
               << " to=" << pToDeck->group
               << " gap=" << tandaGapSecondsFor(pFromDeck, pToDeck)
               << " entry=" << m_tandaEntrySecond
               << " toCortina=" << yesNo(isCortina(pToTrack))
               << " cortinaFade=" << yesNo(m_cortinaFadeEnabled);

    pFromDeck->stop();
    pFromDeck->isFromDeck = false;
    pFromDeck->fadeBeginPos = 1.0;
    pFromDeck->fadeEndPos = 1.0;
    pToDeck->isFromDeck = true;
    pToDeck->setPlayPosition(m_tandaEntrySecond / toDuration);

    const int gapMs = std::max(0, tandaGapSecondsFor(pFromDeck, pToDeck)) * 1000;
    startTandaCrossfaderAnimation(pFromDeck, pToDeck, gapMs);
    if (gapMs == 0) {
        slotTandaGapElapsed();
        return;
    }
    m_tandaGapTimer.start(gapMs);
}

void AutoDJProcessor::cancelTandaGap() {
    if (isTandaGapPending()) {
        TT_TRACE() << "tanda gap cancel";
    }
    m_tandaGapTimer.stop();
    stopTandaCrossfaderAnimation();
    m_pTandaFromDeck = nullptr;
    m_pTandaToDeck = nullptr;
    m_tandaToTrackId = TrackId();
    m_tandaEntrySecond = 0.0;
}

void AutoDJProcessor::slotTandaGapElapsed() {
    DeckAttributes* pFromDeck = m_pTandaFromDeck;
    DeckAttributes* pToDeck = m_pTandaToDeck;
    const TrackId toTrackId = m_tandaToTrackId;
    const double entrySecond = m_tandaEntrySecond;

    TT_TRACE() << "tanda gap elapsed from=" << (pFromDeck ? pFromDeck->group : QString())
               << " to=" << (pToDeck ? pToDeck->group : QString())
               << " entry=" << entrySecond;

    if (!pFromDeck || !pToDeck || m_eState != ADJ_IDLE || pToDeck->loading) {
        cancelTandaGap();
        return;
    }

    const TrackPointer pToTrack = pToDeck->getLoadedTrack();
    const double toDuration = getEndSecond(pToDeck);
    if (!pToTrack || TrackId(pToTrack->getId()) != toTrackId ||
            pToDeck->isPlaying() || toDuration < kMinimumTrackDurationSec) {
        cancelTandaGap();
        return;
    }

    const bool fadeInCortina = m_cortinaFadeEnabled && isCortina(pToTrack);
    if (fadeInCortina) {
        m_pCortinaDeck = pToDeck;
        m_cortinaTrackId = toTrackId;
        m_cortinaFadePhase = CortinaFadePhase::Envelope;
        m_cortinaEnvelopeStartSecond = entrySecond;
        // Start the cortina faded out. maybeHandleCortinaFade() will ramp the
        // internal Auto DJ gain in as callbacks arrive from the cortina deck.
        setAutoDJFadeGain(pToDeck, 0.0);
    } else {
        setAutoDJFadeGain(pToDeck, 1.0);
    }
    setCrossfader(pToDeck->isLeft() ? -1.0 : 1.0);

    pToDeck->setPlayPosition(entrySecond / toDuration);
    pToDeck->play();
    removeLoadedTrackFromTopOfQueue(*pToDeck);
    loadNextTrackFromQueue(*pFromDeck);
    cancelTandaGap();
}
void AutoDJProcessor::useFixedFadeTime(
        DeckAttributes* pFromDeck,
        DeckAttributes* pToDeck,
        double fromDeckSecond,
        double fadeEndSecond,
        double toDeckStartSecond) {
    TT_TRACE() << "useFixedFadeTime from=" << pFromDeck->group
               << " to=" << pToDeck->group
               << " transitionTime=" << m_transitionTime
               << " fromSecond=" << fromDeckSecond
               << " fadeEndSecond=" << fadeEndSecond
               << " toDeckStartSecond=" << toDeckStartSecond;
    if (m_transitionTime > 0.0) {
        // Guard against the next track being too short. This transition must finish
        // before the next transition starts.
        double toDeckOutroStart = pToDeck->fadeBeginPos;
        if (pToDeck->fadeBeginPos >= pToDeck->fadeEndPos) {
            // no outro defined, the toDeck will also use the transition time
            toDeckOutroStart -= m_transitionTime;
        }
        if (toDeckOutroStart <= toDeckStartSecond + kMinimumTrackDurationSec) {
            // we have already passed the outro start
            // Check OutroEnd as alternative, which is for all transition mode
            // better than directly default to duration()
            double end = getOutroEndSecond(pToDeck);
            if (end <= toDeckStartSecond + kMinimumTrackDurationSec) {
                // we have also passed the outro end
                end = getEndSecond(pToDeck);
                VERIFY_OR_DEBUG_ASSERT(end > toDeckStartSecond + kMinimumTrackDurationSec) {
                    // as last resort move start point
                    // The caller makes sure that this never happens
                    toDeckStartSecond = end - kMinimumTrackDurationSec;
                }
            }
            // use the remaining time for fading
            toDeckOutroStart = (end - toDeckStartSecond) / 2 + toDeckStartSecond;
        }
        double transitionTime = math_min(toDeckOutroStart - toDeckStartSecond,
                m_transitionTime);
        VERIFY_OR_DEBUG_ASSERT(transitionTime >= kMinimumTrackDurationSec / 2) {
            transitionTime = kMinimumTrackDurationSec / 2;
        }
        // Note: pFromDeck->fadeBeginPos >= pFromDeck->fadeEndPos is handled in
        // playerPositionChanged() causing a jump cut.
        pFromDeck->fadeBeginPos = math_max(fadeEndSecond - transitionTime, fromDeckSecond);
        pFromDeck->fadeEndPos = fadeEndSecond;
        pToDeck->startPos = toDeckStartSecond;
    } else {
        pFromDeck->fadeBeginPos = fadeEndSecond;
        pFromDeck->fadeEndPos = fadeEndSecond;
        pToDeck->startPos = toDeckStartSecond + m_transitionTime;
    }
    TT_TRACE() << "useFixedFadeTime result fromFadeBegin=" << pFromDeck->fadeBeginPos
               << " fromFadeEnd=" << pFromDeck->fadeEndPos
               << " toStart=" << pToDeck->startPos;
}

void AutoDJProcessor::playerTrackLoaded(DeckAttributes* pDeck, TrackPointer pTrack) {
    if constexpr (sDebug) {
        qDebug() << this << "playerTrackLoaded" << pDeck->group
                 << (pTrack ? pTrack->getLocation() : "(null)");
    }

    pDeck->loading = false;

    // Since the end position is measured in seconds from 0:00 it is also
    // the track duration.
    double duration = getEndSecond(pDeck);
    if (duration < kMinimumTrackDurationSec) {
        qWarning() << "Skip track with" << duration << "Duration"
                   << pTrack->getLocation();
        // Remove Tack with duration smaller than two callbacks
        removeTrackFromTopOfQueue(pTrack);

        // Load the next track. If we are the first AutoDJ track
        // (ADJ_ENABLE_P1LOADED state) then play the track.
        loadNextTrackFromQueue(*pDeck, m_eState == ADJ_ENABLE_P1LOADED);
    } else if (m_eState == ADJ_IDLE) {
        // this deck has just changed the track so it becomes the toDeck
        DeckAttributes* fromDeck = getOtherDeck(pDeck);
        // check if this deck has suitable alignment
        if (fromDeck && getOtherDeck(fromDeck) != pDeck) {
            if constexpr (sDebug) {
                qDebug() << this << "playerTrackLoaded()" << pDeck->group << "but not a toDeck";
            }
            // User has changed the orientation, disable Auto DJ
            toggleAutoDJ(false);
            emit autoDJError(ADJ_NOT_TWO_DECKS);
            return;
        }
        pDeck->startPos = kKeepPosition;
        updatePauseAfterDeckControl();
        calculateTransition(fromDeck, pDeck, true);
        if (pDeck->startPos != kKeepPosition) {
            // Note: this seek will trigger the playerPositionChanged slot
            // which may calls the calculateTransition() again without seek = true;
            pDeck->setPlayPosition(pDeck->startPos);
        }
        // we are her in the relative domain 0..1
        if (!fromDeck->isPlaying() && fromDeck->playPosition() >= 1.0) {
            // repeat a probably missed update
            playerPositionChanged(fromDeck, 1.0);
        }
    } else if (m_eState == ADJ_LEFT_FADING) {
        if (pDeck == getRightDeck()) {
            // restore the play state lost during loading
            pDeck->play();
        }
    } else if (m_eState == ADJ_RIGHT_FADING) {
        if (pDeck == getLeftDeck()) {
            // restore the play state lost during loading
            pDeck->play();
        }
    }
}

void AutoDJProcessor::playerLoadingTrack(DeckAttributes* pDeck,
        TrackPointer pNewTrack, TrackPointer pOldTrack) {
    if constexpr (sDebug) {
        qDebug() << this << "playerLoadingTrack" << pDeck->group
                 << "new:" << (pNewTrack ? pNewTrack->getLocation() : "(null)")
                 << "old:" << (pOldTrack ? pOldTrack->getLocation() : "(null)");
    }

    pDeck->loading = true;

    // The Deck is loading an new track

    // There are four conditions under which we load a track.
    // 1) We are enabling AutoDJ and no decks are playing. Mode is
    //    ADJ_ENABLE_P1LOADED.
    // 2) After #1, we load a track into the other deck. Mode is ADJ_IDLE.
    // 3) We are enabling AutoDJ and a single deck is playing. Mode is ADJ_IDLE.
    // 4) We have just completed fading from one deck to another. Mode is
    //    ADJ_IDLE.

    if (!pNewTrack) {
        // If a track is ejected because of a manual eject command or a load failure
        // this track seams to be undesired. Remove the bad track from the queue.
        removeTrackFromTopOfQueue(pOldTrack);

        // wait until the track is fully unloaded and the playerEmpty()
        // slot is called before load an alternative track.
    }
}

void AutoDJProcessor::playerEmpty(DeckAttributes* pDeck) {
    if constexpr (sDebug) {
        qDebug() << this << "playerEmpty()" << pDeck->group;
    }
    pDeck->loading = false;

    if (isTandaGapPending() &&
            (pDeck == m_pTandaFromDeck || pDeck == m_pTandaToDeck)) {
        cancelTandaGap();
        toggleAutoDJ(false);
        return;
    }

    // The Deck has ejected a track and no new one is loaded
    // This happens if loading fails or the user manually ejected the track
    // and would normally stop the AutoDJ flow, which is not desired.
    // It should be safe to load a new track from the queue. The only case where
    // we request a load-and-play is case #1 currently so we can easily test for
    // this based on the mode.

    // Load the next track. If we are the first AutoDJ track
    // (ADJ_ENABLE_P1LOADED state) then play the track.
    loadNextTrackFromQueue(*pDeck, m_eState == ADJ_ENABLE_P1LOADED);
}

void AutoDJProcessor::playerRateChanged(DeckAttributes* pAttributes) {
    if constexpr (sDebug) {
        qDebug() << this << "playerRateChanged" << pAttributes->group;
    }

    if (m_eState != ADJ_IDLE) {
        // We don't want to recalculate a running transition
        return;
    }

    DeckAttributes* fromDeck = getFromDeck();
    if (!fromDeck) {
        return;
    }
    calculateTransition(fromDeck, getOtherDeck(fromDeck), false);
}

void AutoDJProcessor::playlistFirstTrackChanged() {
    if constexpr (sDebug) {
        qDebug() << this << "playlistFirstTrackChanged";
    }
    if (keepQueueEnabled()) {
        // In Keep Queue mode the next track is at the cursor, not the top of the
        // queue, so reanchorKeepQueueCursor() handles reloading the idle deck.
        return;
    }
    if (m_eState != ADJ_DISABLED) {
        DeckAttributes* pLeftDeck = getLeftDeck();
        DeckAttributes* pRightDeck = getRightDeck();

        if (!pLeftDeck->isPlaying()) {
            loadNextTrackFromQueue(*pLeftDeck);
        } else if (!pRightDeck->isPlaying()) {
            loadNextTrackFromQueue(*pRightDeck);
        }
    }
}

AutoDJProcessor::TransitionMode AutoDJProcessor::lastStockTransitionMode() const {
    return validStockTransitionMode(m_pConfig->getValue(
            ConfigKey(kPreferenceGroup, kLastStockTransitionModePreferenceName),
            static_cast<int>(TransitionMode::FixedSkipSilence)));
}

void AutoDJProcessor::rememberStockTransitionMode(TransitionMode mode) {
    if (!isStockTransitionMode(mode)) {
        return;
    }
    m_pConfig->set(ConfigKey(kPreferenceGroup, kLastStockTransitionModePreferenceName),
            ConfigValue(static_cast<int>(mode)));
}

void AutoDJProcessor::restoreLastStockTransitionMode() {
    setTransitionMode(lastStockTransitionMode());
}

void AutoDJProcessor::setTransitionTime(int time) {
    if constexpr (sDebug) {
        qDebug() << this << "setTransitionTime" << time;
    }

    // Update the transition time first.
    m_pConfig->setValue(ConfigKey(kPreferenceGroup, kTransitionPreferenceName),
            time);
    m_transitionTime = time;

    // Then re-calculate fade thresholds for the decks.
    if (m_eState == ADJ_IDLE) {
        DeckAttributes* pLeftDeck = getLeftDeck();
        DeckAttributes* pRightDeck = getRightDeck();
        if (!pLeftDeck || !pRightDeck) {
            // User has changed the orientation, disable Auto DJ
            toggleAutoDJ(false);
            emit autoDJError(ADJ_NOT_TWO_DECKS);
            return;
        }
        if (pLeftDeck->isPlaying()) {
            calculateTransition(pLeftDeck, pRightDeck, false);
        }
        if (pRightDeck->isPlaying()) {
            calculateTransition(pRightDeck, pLeftDeck, false);
        }
    }
}

void AutoDJProcessor::setTandaGapSeconds(int seconds) {
    const int boundedSeconds = std::max(0, seconds);
    m_pConfig->setValue(ConfigKey(kPreferenceGroup, kTandaGapPreferenceName),
            boundedSeconds);
    if (m_tandaGapSeconds == boundedSeconds) {
        return;
    }
    m_tandaGapSeconds = boundedSeconds;
    m_cortinaGapSeconds = boundedSeconds;
    invalidateRemainingSetDuration();

    if (m_eState != ADJ_IDLE || isTandaGapPending()) {
        return;
    }

    DeckAttributes* pFromDeck = getFromDeck();
    if (pFromDeck) {
        calculateTransition(pFromDeck, getOtherDeck(pFromDeck), false);
    }
}
void AutoDJProcessor::setTransitionMode(TransitionMode newMode) {
    if (isStockTransitionMode(newMode)) {
        rememberStockTransitionMode(newMode);
    }
    m_pConfig->set(ConfigKey(kPreferenceGroup, kTransitionModePreferenceName),
            ConfigValue(static_cast<int>(newMode)));
    m_transitionMode = newMode;
    // Switching to/from Skip Silence changes whether the set estimate uses each
    // track's audible range or its full file length, so the cache is now stale.
    invalidateRemainingSetDuration();

    if (m_eState != ADJ_IDLE) {
        // We don't want to recalculate a running transition
        return;
    }

    // Then re-calculate fade thresholds for the decks.
    DeckAttributes* pLeftDeck = getLeftDeck();
    DeckAttributes* pRightDeck = getRightDeck();

    if (!pLeftDeck || !pRightDeck) {
        // User has changed the orientation, disable Auto DJ
        toggleAutoDJ(false);
        emit autoDJError(ADJ_NOT_TWO_DECKS);
        return;
    }

    if (pLeftDeck->isPlaying() && !pRightDeck->isPlaying()) {
        calculateTransition(pLeftDeck, pRightDeck, true);
        if (pRightDeck->startPos != kKeepPosition) {
            // Note: this seek will trigger the playerPositionChanged slot
            // which may calls the calculateTransition() again without seek = true;
            pRightDeck->setPlayPosition(pRightDeck->startPos);
        }
    } else if (pRightDeck->isPlaying() && pLeftDeck->isPlaying()) {
        calculateTransition(pRightDeck, pLeftDeck, true);
        if (pLeftDeck->startPos != kKeepPosition) {
            // Note: this seek will trigger the playerPositionChanged slot
            // which may calls the calculateTransition() again without seek = true;
            pLeftDeck->setPlayPosition(pLeftDeck->startPos);
        }
    } else {
        // user has manually started the other deck or stopped both.
        // don't know what to do.
    }
}

DeckAttributes* AutoDJProcessor::getLeftDeck() {
    // find first left deck
    for (const auto& pDeck : m_decks) {
        if (pDeck->isLeft()) {
            return pDeck.get();
        }
    }
    return nullptr;
}

DeckAttributes* AutoDJProcessor::getRightDeck() {
    // find first right deck
    for (const auto& pDeck : m_decks) {
        if (pDeck->isRight()) {
            return pDeck.get();
        }
    }
    return nullptr;
}

DeckAttributes* AutoDJProcessor::getOtherDeck(
        const DeckAttributes* pThisDeck) {
    if (pThisDeck->isLeft()) {
        return getRightDeck();
    }
    if (pThisDeck->isRight()) {
        return getLeftDeck();
    }
    return nullptr;
}

DeckAttributes* AutoDJProcessor::getFromDeck() {
    for (const auto& pDeck : m_decks) {
        if (pDeck->isFromDeck) {
            return pDeck.get();
        }
    }
    return nullptr;
}

bool AutoDJProcessor::nextTrackLoaded() {
    if (m_eState == ADJ_DISABLED) {
        // AutoDJ always loads the top track (again) if enabled
        return false;
    }

    DeckAttributes* pLeftDeck = getLeftDeck();
    DeckAttributes* pRightDeck = getRightDeck();
    if (!pLeftDeck || !pRightDeck) {
        return false;
    }

    bool leftDeckPlaying = pLeftDeck->isPlaying();
    bool rightDeckPlaying = pRightDeck->isPlaying();

    // Calculate idle deck
    TrackPointer loadedTrack;
    if (leftDeckPlaying && !rightDeckPlaying) {
        loadedTrack = pRightDeck->getLoadedTrack();
    } else if (!leftDeckPlaying && rightDeckPlaying) {
        loadedTrack = pLeftDeck->getLoadedTrack();
    } else if (getCrossfader() < 0.0) {
        loadedTrack = pRightDeck->getLoadedTrack();
    } else {
        loadedTrack = pLeftDeck->getLoadedTrack();
    }

    return loadedTrack == getNextTrackFromQueue();
}

int AutoDJProcessor::firstUnloadedQueuePosition() {
    if (m_eState == ADJ_DISABLED) {
        return 1;
    }
    // m_keepQueueRow is zero-based and names the next queue row. If that row is
    // already cued on the idle deck, protection begins with the row after it.
    return m_keepQueueRow + 1 + (nextTrackLoaded() ? 1 : 0);
}

int AutoDJProcessor::activeKeepQueuePosition() {
    if (!keepQueueEnabled() || m_eState == ADJ_DISABLED) {
        return 0;
    }

    auto positionForDeck = [this](DeckAttributes* pDeck, int rowGuess) {
        if (!pDeck) {
            return 0;
        }
        const int row = keepQueueRowForDeck(pDeck);
        if (row >= 0) {
            return row + 1;
        }
        const int fallbackRow =
                keepQueueRowForTrack(pDeck->getLoadedTrack(), rowGuess);
        return fallbackRow >= 0 ? fallbackRow + 1 : 0;
    };

    // During a Tanda Transition gap the outgoing deck has been stopped and the
    // incoming deck has not started yet. Keep the active tanda anchored on the
    // outgoing queue row until the next track actually starts.
    if (isTandaGapPending()) {
        const int position = positionForDeck(m_pTandaFromDeck, m_keepQueueRow - 1);
        if (position > 0) {
            return position;
        }
    }

    if (DeckAttributes* pFromDeck = getFromDeck()) {
        const int position = positionForDeck(pFromDeck, m_keepQueueRow - 1);
        if (position > 0) {
            return position;
        }
    }

    for (const auto& pDeck : m_decks) {
        if (pDeck && pDeck->isPlaying()) {
            const int position = positionForDeck(pDeck.get(), m_keepQueueRow - 1);
            if (position > 0) {
                return position;
            }
        }
    }

    if (m_keepQueueAnchorId.isValid()) {
        const int row = keepQueueRowForTrackId(m_keepQueueAnchorId, m_keepQueueRow - 1);
        if (row >= 0) {
            return row + 1;
        }
    }

    return 0;
}


