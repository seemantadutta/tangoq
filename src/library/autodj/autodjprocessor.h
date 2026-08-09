#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>
#include <vector>

#include "audio/frame.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "control/pollingcontrolproxy.h"
#include "engine/channels/enginechannel.h"
#include "library/playlisttablemodel.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"
#include "track/trackid.h"
#include "util/class.h"
#include "util/duration.h"
#include "util/parented_ptr.h"

class TrackCollectionManager;
class PlayerManagerInterface;
class BaseTrackPlayer;
typedef QList<QModelIndex> QModelIndexList;

class DeckAttributes : public QObject {
    Q_OBJECT
  public:
    DeckAttributes(int index,
            BaseTrackPlayer* pPlayer);
    virtual ~DeckAttributes();

    bool isLeft() const {
        return m_orientation.get() == static_cast<double>(EngineChannel::LEFT);
    }

    bool isRight() const {
        return m_orientation.get() == static_cast<double>(EngineChannel::RIGHT);
    }

    bool isPlaying() const {
        return m_play.toBool();
    }

    void stop() {
        m_play.set(0.0);
    }

    void play() {
        m_play.set(1.0);
    }

    double playPosition() const {
        return m_playPos.get();
    }

    void setPlayPosition(double playpos) {
        m_playPos.set(playpos);
    }

    bool isRepeat() const {
        return m_repeat.toBool();
    }

    void setRepeat(bool enabled) {
        m_repeat.set(enabled ? 1.0 : 0.0);
    }

    mixxx::audio::FramePos introStartPosition() const {
        return mixxx::audio::FramePos::fromEngineSamplePosMaybeInvalid(m_introStartPos.get());
    }

    mixxx::audio::FramePos introEndPosition() const {
        return mixxx::audio::FramePos::fromEngineSamplePosMaybeInvalid(m_introEndPos.get());
    }

    mixxx::audio::FramePos outroStartPosition() const {
        return mixxx::audio::FramePos::fromEngineSamplePosMaybeInvalid(m_outroStartPos.get());
    }

    mixxx::audio::FramePos outroEndPosition() const {
        return mixxx::audio::FramePos::fromEngineSamplePosMaybeInvalid(m_outroEndPos.get());
    }

    mixxx::audio::SampleRate sampleRate() const {
        return mixxx::audio::SampleRate::fromDouble(m_sampleRate.get());
    }

    mixxx::audio::FramePos trackEndPosition() const {
        return mixxx::audio::FramePos::fromEngineSamplePosMaybeInvalid(m_trackSamples.get());
    }

    double rateRatio() const {
        return m_rateRatio.get();
    }

    TrackPointer getLoadedTrack() const;

  signals:
    void playChanged(DeckAttributes* pDeck, bool playing);
    void playPositionChanged(DeckAttributes* pDeck, double playPosition);
    void introStartPositionChanged(DeckAttributes* pDeck, double introStartPosition);
    void introEndPositionChanged(DeckAttributes* pDeck, double introEndPosition);
    void outroStartPositionChanged(DeckAttributes* pDeck, double outtroStartPosition);
    void outroEndPositionChanged(DeckAttributes* pDeck, double outroEndPosition);
    void trackLoaded(DeckAttributes* pDeck, TrackPointer pTrack);
    void loadingTrack(DeckAttributes* pDeck, TrackPointer pNewTrack, TrackPointer pOldTrack);
    void playerEmpty(DeckAttributes* pDeck);
    void rateChanged(DeckAttributes* pDeck);

  private slots:
    void slotPlayPosChanged(double v);
    void slotPlayChanged(double v);
    void slotIntroStartPositionChanged(double v);
    void slotIntroEndPositionChanged(double v);
    void slotOutroStartPositionChanged(double v);
    void slotOutroEndPositionChanged(double v);
    void slotTrackLoaded(TrackPointer pTrack);
    void slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack);
    void slotPlayerEmpty();
    void slotRateChanged(double v);

  public:
    int index;
    QString group;
    double startPos;     // Set in toDeck nature
    double fadeBeginPos; // set in fromDeck nature
    double fadeEndPos;   // set in fromDeck nature
    bool isFromDeck;
    bool loading; // The data is inconsistent during loading a deck

  private:
    ControlProxy m_orientation;
    ControlProxy m_playPos;
    ControlProxy m_play;
    ControlProxy m_repeat;
    ControlProxy m_introStartPos;
    ControlProxy m_introEndPos;
    ControlProxy m_outroStartPos;
    ControlProxy m_outroEndPos;
    ControlProxy m_trackSamples;
    ControlProxy m_sampleRate;
    ControlProxy m_rateRatio;
    BaseTrackPlayer* m_pPlayer;
};

class AutoDJProcessor : public QObject {
    Q_OBJECT
  public:
    enum AutoDJState {
        ADJ_IDLE = 0,
        ADJ_LEFT_FADING,
        ADJ_RIGHT_FADING,
        ADJ_ENABLE_P1LOADED,
        ADJ_ENABLE_P1PLAYING,
        ADJ_DISABLED
    };

    enum AutoDJError {
        ADJ_OK = 0,
        ADJ_IS_INACTIVE,
        ADJ_QUEUE_EMPTY,
        ADJ_BOTH_DECKS_PLAYING,
        ADJ_UNUSED_DECK_PLAYING,
        ADJ_NOT_TWO_DECKS
    };

    enum class TransitionMode {
        FullIntroOutro,
        FadeAtOutroStart,
        FixedFullTrack,
        FixedSkipSilence,
        TandaTransition
    };

    AutoDJProcessor(QObject* pParent,
                    UserSettingsPointer pConfig,
                    PlayerManagerInterface* pPlayerManager,
                    TrackCollectionManager* pTrackCollectionManager,
                    int iAutoDJPlaylistId);
    virtual ~AutoDJProcessor() = default;

    AutoDJState getState() const {
        return m_eState;
    }

    // Tango DJ mode set timing: the estimated remaining playback time of the
    // current Auto DJ set, i.e. the unplayed part of the current (playing or
    // paused) track plus all upcoming tracks from the cursor onward, adjusted for
    // the configured gap/crossfade. In Skip Silence mode the per-track estimate
    // uses the analyzed audible range (N60dBSound cue) instead of the full file
    // length, so trimmed leading/trailing silence is not over-counted. The
    // expensive upcoming-tracks sum is cached and only recomputed when the queue,
    // cursor or transition mode changes, so the per-second UI refresh is cheap.
    // Returns an empty Duration when not in Tango mode or when nothing is left.
    mixxx::Duration getRemainingSetDuration();

    // Tango DJ mode set timing: the estimated total playback time of the whole
    // Auto DJ set, i.e. every queued track (played and upcoming) summed with the
    // same audible-range / gap accounting as getRemainingSetDuration(). Unlike
    // the remaining duration this does not depend on the cursor or play position,
    // so it stays constant while the set plays and only changes when the queue or
    // transition mode is edited. Shares the same lazily recomputed cache.
    // Returns an empty Duration when not in Tango mode or when the queue is empty.
    mixxx::Duration getTotalSetDuration();

    double getTransitionTime() const {
        return m_transitionTime;
    }

    TransitionMode getTransitionMode() const {
        return m_transitionMode;
    }

    PlaylistTableModel* getTableModel() const {
        return m_pAutoDJTableModel;
    }

    bool nextTrackLoaded();

    /// Test seam for the stop decision. The two callers reach it from deep
    /// inside a transition or a gap timer, neither of which the fake decks can
    /// drive faithfully, so the rule itself is exercised directly.
    bool shouldStopAfterRowForTest(int row) {
        return shouldStopAfterRow(row);
    }

    /// True while Tango (Keep Queue) mode owns the Auto DJ queue: the list is a
    /// pre-arranged set and the cursor tracks the DJ's position in it. Callers
    /// that would reorder or clear the queue must refuse while this holds.
    bool isQueueOrderLocked() const;

    void setTransitionTime(int seconds);

    void setTransitionMode(TransitionMode newMode);

    AutoDJError shufflePlaylist(const QModelIndexList& selectedIndices);
    AutoDJError skipNext();
    void fadeNow();
    AutoDJError toggleAutoDJ(bool enable);

  signals:
    void loadTrackToPlayer(TrackPointer pTrack, const QString& group, bool play);
    void autoDJStateChanged(AutoDJProcessor::AutoDJState state);
    void autoDJError(AutoDJProcessor::AutoDJError error);
    void transitionTimeChanged(int time);
    void randomTrackRequested(int tracksToAdd);
    // Emitted when the LIVE-mode accidental-stop guard arms/disarms so the toolbar
    // can show a "Confirm Stop?" prompt on the Auto DJ button.
    void stopGuardArmedChanged(bool armed);

  private slots:
    void crossfaderChanged(double value);
    void playerPositionChanged(DeckAttributes* pDeck, double position);
    void playerPlayChanged(DeckAttributes* pDeck, bool playing);
    void playerIntroStartChanged(DeckAttributes* pDeck, double position);
    void playerIntroEndChanged(DeckAttributes* pDeck, double position);
    void playerOutroStartChanged(DeckAttributes* pDeck, double position);
    void playerOutroEndChanged(DeckAttributes* pDeck, double position);
    void playerTrackLoaded(DeckAttributes* pDeck, TrackPointer pTrack);
    void playerLoadingTrack(DeckAttributes* pDeck, TrackPointer pNewTrack, TrackPointer pOldTrack);
    void playerEmpty(DeckAttributes* pDeck);
    void playerRateChanged(DeckAttributes* pDeck);
    void playlistFirstTrackChanged();

    void controlEnableChangeRequest(double value);
    void controlFadeNow(double value);
    void controlShuffle(double value);
    void controlSkipNext(double value);
    void controlAddRandomTrack(double value);
    void controlKeepQueue(double value);
    /// Gate for every route into Tango mode - the preferences checkbox, the
    /// keyboard shortcut and any controller mapping - so none of them can switch
    /// it while Auto DJ is running.
    void controlKeepQueueChangeRequest(double value);
    // Persists a live cortina-length change (from the cockpit nudge buttons or
    // the prefs Apply) to config and refreshes the envelope budget + estimate.
    void controlCortinaLength(double value);
    // Triggered from the Auto DJ queue right-click "Eject decks and reset AutoDJ
    // queue state" action to restart the Tango set from the top (see
    // resetKeepQueueSet). The deck eject is done by the menu action itself.
    void controlResetQueueState(double value);
    // Cancels the LIVE-mode stop-guard arm (timeout or a non-confirming action).
    void disarmStopGuard();
    // Fires when a cortina's Nc silent gap has elapsed: resumes the paused
    // cortina (before-gap) or hard-starts the next tanda track (after-gap).
    void slotCortinaGapElapsed();
    void slotNumberOfDecksChanged(int decks);

  protected:
    // The following virtual signal wrappers are used for testing
    virtual void emitLoadTrackToPlayer(TrackPointer pTrack, const QString& group, bool play) {
        emit loadTrackToPlayer(pTrack, group, play);
    }
    virtual void emitAutoDJStateChanged(AutoDJProcessor::AutoDJState state) {
        emit autoDJStateChanged(state);
    }

  private:
    // Gets or sets the crossfader position while normalizing it so that -1 is
    // all the way mixed to the left side and 1 is all the way mixed to the
    // right side. (prevents AutoDJ logic from having to check for hamster mode
    // every time)
    double getCrossfader() const;
    void setCrossfader(double value);

    // Following functions return seconds computed from samples or -1 if
    // track in deck has invalid sample rate (<= 0)
    double getIntroStartSecond(DeckAttributes* pDeck);
    double getIntroEndSecond(DeckAttributes* pDeck);
    double getOutroStartSecond(DeckAttributes* pDeck);
    double getOutroEndSecond(DeckAttributes* pDeck);
    double getFirstSoundSecond(DeckAttributes* pDeck);
    double getLastSoundSecond(DeckAttributes* pDeck);
    double getEndSecond(DeckAttributes* pDeck);
    double framePositionToSeconds(mixxx::audio::FramePos position, DeckAttributes* pDeck);

    TrackPointer getNextTrackFromQueue();
    bool loadNextTrackFromQueue(const DeckAttributes& pDeck, bool play = false);
    void calculateTransition(DeckAttributes* pFromDeck,
            DeckAttributes* pToDeck,
            bool seekToStartPoint);
    void useFixedFadeTime(
            DeckAttributes* pFromDeck,
            DeckAttributes* pToDeck,
            double fromDeckSecond,
            double fadeEndSecond,
            double toDeckStartSecond);
    DeckAttributes* getLeftDeck();
    DeckAttributes* getRightDeck();
    DeckAttributes* getOtherDeck(const DeckAttributes* pThisDeck);
    DeckAttributes* getFromDeck();

    // Removes the track loaded to the player group from the top of the AutoDJ
    // queue if it is present.
    bool removeLoadedTrackFromTopOfQueue(const DeckAttributes& deck);

    // Removes the provided track from the top of the AutoDJ queue if it is
    // present.
    bool removeTrackFromTopOfQueue(TrackPointer pTrack);

    // Restarts the Tango set from the top: marks every queued track unplayed
    // (clearing the grey "played" colour, but keeping the user's play counts) and
    // resets the play cursor to row 0. Only acts while Auto DJ is stopped and in
    // Keep Queue mode. Lets a fully-played set be replayed.
    void resetKeepQueueSet();

    // True if any Auto DJ deck is currently playing.
    bool anyDeckPlaying() const;

    /// True while Auto DJ is stopping itself rather than being stopped by the
    /// DJ - the end of the queue, or a marked announcement pause. Such a stop
    /// must not be held behind the LIVE confirmation guard, which exists to
    /// catch stray presses.
    bool automaticStopPending() const {
        return m_bStopWhenLastTrackEnds || m_bPauseAfterPending;
    }

    /// Stays enabled with nothing left to hand over to: arms the stop that
    /// playerPlayChanged() carries out once the playing track ends, and
    /// neutralises every pending transition so nothing starts behind our back.
    void armStopWhenLastTrackEnds();

    /// Whether the set should stop after the track on this row - i.e. whether
    /// the DJ marked it. Shared by both places a transition is claimed so they
    /// cannot diverge; the caller consumes the mark.
    bool shouldStopAfterRow(int row);

    /// Claims the transition when the playing deck sits on a "pause after" row:
    /// consumes the mark, arms the stop and leaves the track to play out.
    /// Returns true if the caller should abandon the transition.
    bool maybeHoldForAnnouncement(DeckAttributes* pDeck);

    /// Publishes which deck holds a pending pause so its title can warn. Rows
    /// are positional, so a deck cannot work this out from its own track.
    void updatePauseAfterDeckControl();

    /// Row of the track loaded on pDeck, or -1. Uses the cursor as the guess so
    /// a repeated cortina resolves to the occurrence actually playing.
    int keepQueueRowForDeck(DeckAttributes* pDeck);

    // Keep Queue mode helpers.
    bool keepQueueEnabled() const;
    // True while LIVE mode (Tango performance lock) is engaged.
    bool liveModeEnabled() const;
    // Advances the Keep Queue cursor past pTrack (instead of removing it) if it
    // is the current cursor track. Returns true if the cursor advanced.
    bool advanceKeepQueueCursor(TrackPointer pTrack);
    // Re-anchors the Keep Queue cursor to the currently playing track after the
    // queue is edited, so it keeps pointing at the correct next track even when
    // tracks are added, removed or reordered while Auto DJ is running.
    void reanchorKeepQueueCursor();
    // Reloads the idle deck if a queue edit changed which track is next, so a
    // deleted or reordered cued track isn't left loaded and played off-list.
    void maybeReloadIdleDeckForKeepQueue();
    // Returns the model row of pTrack nearest rowGuess (to disambiguate
    // duplicates), or -1 if pTrack is not in the queue.
    int keepQueueRowForTrack(TrackPointer pTrack, int rowGuess);
    int keepQueueRowForTrackId(TrackId trackId, int rowGuess);

    // Last stock transition mode selected by the DJ. Tanda Transition is Tango-only,
    // so leaving Tango restores this mode instead of leaving a hidden option active.
    TransitionMode lastStockTransitionMode() const;
    void rememberStockTransitionMode(TransitionMode mode);
    void restoreLastStockTransitionMode();

    // Set-duration cache (see getRemainingSetDuration). Recomputes the upcoming
    // tracks' total play time only when something that affects it changes.
    void recomputeKeepQueueUpcomingDuration();
    void invalidateRemainingSetDuration() {
        m_keepQueueDurationDirty = true;
    }
    // Picks up changed cortina preferences (length, and the Cortina Fade
    // settings while Auto DJ is stopped) and recomputes the cached sums when
    // stale.
    void refreshSetDurationCacheIfNeeded();
    // True if the track is tagged as a cortina (faded out manually, so the set
    // estimate budgets only the configured cortina length for it).
    bool isCortina(const TrackPointer& pTrack) const;
    // Cortina Fade transition driver (Tango mode only). When Cortina Fade mode
    // is on and a cortina plays solo in ADJ_IDLE, it runs a small phase machine:
    // Nc silent before-gap (cortina paused at its first sound), then a crossfader
    // envelope as a pure function of the elapsed audible time (ramp in over X,
    // hold for Y, ramp out over Z against the stopped partner deck), then an Nc
    // after-gap (cortina stopped) after which the next tanda track is
    // hard-started. Returns true when it claims the callback (the caller must
    // then skip the normal transition handling for this deck).
    bool maybeHandleCortinaFade(DeckAttributes* thisDeck, double thisPlayPosition);
    // Reads the Cortina Fade settings from [Auto DJ]. Called at construction and
    // when Auto DJ is enabled (they are only editable while it is stopped), so
    // the engine behaviour never depends on the UI refresh timer.
    void loadCortinaFadeSettings();
    // Enters the Nc silent after-gap: parks the crossfader on the next deck's
    // side, stops the cortina and starts the gap timer.
    void startCortinaAfterGap(DeckAttributes* pCortinaDeck);
    // Stops the gap timer and resets all Cortina Fade phase state, handing
    // control back to the normal transition machinery.
    void cancelCortinaFade();
    // Estimated playback seconds an upcoming (not-yet-loaded) track contributes:
    // the cortina budget if tagged, else its audible range in Skip Silence mode,
    // else the full file duration.
    double keepQueueTrackPlaySeconds(const TrackPointer& pTrack) const;
    // Unplayed seconds of the current (playing/paused) track at playPosition,
    // stopping at the last audible sample in Skip Silence mode.
    double keepQueueCurrentTrackRemainingSeconds(
            const TrackPointer& pTrack, double playPosition) const;
    // Audible length in seconds from the analyzed N60dBSound cue, or 0 if the
    // track has no such cue (e.g. not yet analyzed).
    double keepQueueAudibleSeconds(const TrackPointer& pTrack) const;

    void maybeFillRandomTracks();
    UserSettingsPointer m_pConfig;
    parented_ptr<PlaylistTableModel> m_pAutoDJTableModel;

    AutoDJState m_eState;
    double m_transitionProgress;
    double m_transitionTime; // the desired value set by the user
    // Keep Queue ("Tango") mode: 0-based row index of the next track to play.
    // Acts as a cursor so played tracks stay in the list instead of being
    // removed, and Auto DJ stops when it reaches the end.
    int m_keepQueueRow;
    // Identity of the last-played track (sits at cursor-1). Used to re-anchor the
    // cursor across the full model rebuild that every queue edit triggers while
    // Auto DJ is stopped, so adding/removing tracks does not reset it to the top.
    TrackId m_keepQueueAnchorId;
    // Guards against re-entrancy while reloading the idle deck after a queue edit.
    bool m_keepQueueReloading;
    // Cached total play time (seconds) and count of the upcoming tracks
    // (cursor .. end), recomputed lazily when m_keepQueueDurationDirty is set by
    // a queue edit, cursor move or transition-mode change. Keeps the 1 Hz set
    // end-time readout from re-reading every queued track from the database.
    double m_keepQueueUpcomingSeconds;
    int m_keepQueueUpcomingTracks;
    // Cached total play time (seconds) and count of the whole queue (row 0 .. end),
    // recomputed in the same pass as the upcoming sum. Backs the constant "Set
    // Length" readout.
    double m_keepQueueTotalSeconds;
    int m_keepQueueTotalTracks;
    // Cortina play-time budget (seconds) baked into the cached sums above. Read
    // from [Auto DJ]/CortinaLength; a change re-dirties the cache.
    int m_keepQueueCortinaSeconds;
    // Cortina counts in the cached sums, used to exclude cortina boundaries from
    // the Nt transition adjustment when Cortina Fade mode is on (those
    // boundaries use the Nc gaps, which keepQueueTrackPlaySeconds counts).
    int m_keepQueueUpcomingCortinas;
    int m_keepQueueTotalCortinas;
    bool m_keepQueueDurationDirty;
    // Cortina Fade transition settings (Tango mode), snapshot from [Auto DJ] by
    // loadCortinaFadeSettings(). When m_cortinaFadeEnabled is true the engine
    // fades a cortina in over m_cortinaFadeInSeconds (X) and out over
    // m_cortinaFadeOutSeconds (Z); m_cortinaGapSeconds (Nc) is the silent gap
    // before and after the cortina. The hold time Y = cortina length - X - Z is
    // derived. When false, cortinas keep the legacy hard-in / manual fade-out.
    bool m_cortinaFadeEnabled;
    int m_cortinaFadeInSeconds;
    int m_cortinaFadeOutSeconds;
    int m_cortinaGapSeconds;
    // Cortina Fade phase machine (see maybeHandleCortinaFade). The gap timer
    // holds the Nc silence of the current gap phase; m_pCortinaDeck and
    // m_cortinaTrackId pin down the deck/track the phases refer to, so a track
    // or state change under a running phase safely cancels it.
    enum class CortinaFadePhase {
        None,      // inactive; normal transition machinery in charge
        BeforeGap, // cortina paused at its first sound, gap timer running
        Envelope,  // cortina playing; crossfader driven by elapsed time
        AfterGap,  // cortina stopped after fade-out, gap timer running
    };
    CortinaFadePhase m_cortinaFadePhase;
    QTimer m_cortinaGapTimer;
    DeckAttributes* m_pCortinaDeck;
    TrackId m_cortinaTrackId;
    TransitionMode m_transitionMode;

    PlayerManagerInterface* m_pPlayerManager;
    std::vector<std::unique_ptr<DeckAttributes>> m_decks;

    ControlProxy m_coCrossfader;
    PollingControlProxy m_coCrossfaderReverse;

    ControlPushButton m_shufflePlaylist;
    ControlPushButton m_skipNext;
    ControlPushButton m_addRandomTrack;
    ControlPushButton m_fadeNow;
    ControlPushButton m_enabledAutoDJ;
    // Mirrors [Auto DJ],KeepQueue (Tango DJ mode) as a live control so the prefs
    // dialog and the Auto DJ toolbar stay in sync when it changes.
    // A toggle rather than a plain control so a keyboard shortcut or controller
    // button flips it on the rising edge, the way [AutoDJ],enabled works.
    ControlPushButton m_keepQueue;
    // The inverse of m_keepQueue, kept in lockstep with it. Exists purely so
    // skins can hide things *while* Tango mode is on: a waveform
    // <VisibilityControl> takes a single bare ConfigKey with no transform, so it
    // cannot express "not Tango" on its own. Used to drop the main cue marker in
    // Tango, where the start-point marker is the only one that should show.
    ControlObject m_keepQueueOff;
    // Index (1-based) of the deck holding a track marked "pause after", 0 for
    // none. Read by the deck's title widget, which cannot resolve a positional
    // mark by itself.
    ControlObject m_pauseAfterDeck;

    // Live cortina-length budget (seconds). The single source of truth shared by
    // the Preferences field (stop-only) and the cockpit nudge buttons (live). On
    // change it persists to [Auto DJ],CortinaLength and updates the envelope
    // budget + set-length estimate immediately. Clamped to [5, 600] s.
    ControlObject m_cortinaLength;

    // Momentary trigger from the Auto DJ queue "Eject decks and reset AutoDJ
    // queue state" menu action. Re-armed to 0 after each handled trigger.
    ControlPushButton m_resetQueueState;

    // LIVE mode (Tango performance lock). Session-only, not persisted: defaults
    // off at every launch. While on, it arms the accidental-stop guards.
    ControlObject m_liveMode;
    // Stop-guard arm state: in LIVE mode the first disable request only arms a
    // short confirmation window; a second request within it actually stops.
    bool m_stopGuardArmed;
    QTimer m_stopGuardTimer;

    // Set when the queue runs dry while the last track is still playing. Auto DJ
    // used to stop the moment that track *started*, because that is when it looks
    // for a successor to cue up - which reported the set as over while the floor
    // was still dancing, and satisfied "Auto DJ is off" guards too early. Instead
    // stay enabled and stop from playerPlayChanged() once the track really ends.
    bool m_bStopWhenLastTrackEnds;

    // Set when the track now playing sits on a row the DJ marked "pause after",
    // so the set stops for an announcement instead of starting the next tanda.
    // Kept apart from m_bStopWhenLastTrackEnds because that one is cleared
    // whenever a track is loaded from the queue, which would drop the pause if
    // the queue were edited while the marked track was still playing.
    bool m_bPauseAfterPending;

    DISALLOW_COPY_AND_ASSIGN(AutoDJProcessor);
};
