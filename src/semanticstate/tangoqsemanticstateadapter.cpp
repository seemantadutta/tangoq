// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#include "semanticstate/tangoqsemanticstateadapter.h"

#include <QPointer>
#include <QUuid>
#include <algorithm>
#include <cmath>

#include "control/controlproxy.h"
#include "library/autodj/autodjprocessor.h"
#include "library/autodj/cortinaregistry.h"
#include "library/autodj/tandaqueuestate.h"
#include "library/dao/playlistdao.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playermanager.h"
#include "semanticstate/semanticstatestore.h"
#include "semanticstate/tangoqsemanticstateprojector.h"
#include "track/track.h"

namespace mixxx::semanticstate {

namespace {

constexpr int kPositionPublishIntervalMs = 250;

bool affectsPlaylist(const QSet<int>& playlistIds, int playlistId) {
    return playlistIds.contains(playlistId);
}

QString tandaTypeToProtocolString(TandaType type) {
    switch (type) {
    case TandaType::Tango:
        return QStringLiteral("tango");
    case TandaType::Vals:
        return QStringLiteral("vals");
    case TandaType::Milonga:
        return QStringLiteral("milonga");
    case TandaType::NuevoAlternative:
        return QStringLiteral("nuevo-alternative");
    }
    return QStringLiteral("tango");
}

} // namespace

struct TangoQAdapter::DeckObserver {
    QPointer<BaseTrackPlayer> pPlayer;
    std::unique_ptr<ControlProxy> pPlay;
    std::unique_ptr<ControlProxy> pPosition;
};

TangoQAdapter::TangoQAdapter(Store* pStore,
        PlaylistDAO* pPlaylistDao,
        int autoDJPlaylistId,
        AutoDJProcessor* pAutoDJProcessor,
        TandaQueueState* pTandaQueueState,
        PlayerManagerInterface* pPlayerManager,
        QObject* pParent)
        : QObject(pParent),
          m_pStore(pStore),
          m_autoDJPlaylistId(autoDJPlaylistId),
          m_pAutoDJProcessor(pAutoDJProcessor),
          m_pTandaQueueState(pTandaQueueState),
          m_pPlayerManager(pPlayerManager),
          m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces)),
          m_startedAt(QDateTime::currentDateTimeUtc()) {
    observeDecks();
    connect(m_pPlayerManager,
            &PlayerManagerInterface::numberOfDecksChanged,
            this,
            [this](int) {
                observeDecks();
                publish(false);
            },
            Qt::QueuedConnection);

    const auto queueChanged = [this](const QSet<int>& playlistIds) {
        if (affectsPlaylist(playlistIds, m_autoDJPlaylistId)) {
            publish(true);
        }
    };
    connect(pPlaylistDao,
            &PlaylistDAO::tracksAdded,
            this,
            queueChanged,
            Qt::QueuedConnection);
    connect(pPlaylistDao,
            &PlaylistDAO::tracksMoved,
            this,
            queueChanged,
            Qt::QueuedConnection);
    connect(pPlaylistDao,
            &PlaylistDAO::tracksRemoved,
            this,
            queueChanged,
            Qt::QueuedConnection);

    // Track metadata edits are model changes without a queue DAO mutation.
    // Queue the refresh so the model has completed applying the change first.
    auto* pQueueModel = m_pAutoDJProcessor->getTableModel();
    connect(pQueueModel,
            &QAbstractItemModel::dataChanged,
            this,
            [this]() {
                publish(true);
            },
            Qt::QueuedConnection);
    connect(pQueueModel,
            &QAbstractItemModel::modelReset,
            this,
            [this]() {
                publish(true);
            },
            Qt::QueuedConnection);
    connect(
            m_pTandaQueueState,
            &TandaQueueState::spansChanged,
            this,
            [this]() {
                publish(false);
            },
            Qt::QueuedConnection);
    connect(&CortinaRegistry::instance(),
            &CortinaRegistry::cortinaMarksChanged,
            this,
            [this]() {
                publish(false);
            },
            Qt::QueuedConnection);
    connect(m_pAutoDJProcessor,
            &AutoDJProcessor::autoDJStateChanged,
            this,
            [this](AutoDJProcessor::AutoDJState) {
                publish(false);
            },
            Qt::QueuedConnection);

    m_positionTimer.setInterval(kPositionPublishIntervalMs);
    m_positionTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_positionTimer, &QTimer::timeout, this, [this]() {
        publish(false);
    });
    m_positionTimer.start();

    rebuildQueueCache();
    m_pStore->publish(projectState(buildProjectionInput()));
}

TangoQAdapter::~TangoQAdapter() = default;

void TangoQAdapter::observeDecks() {
    for (int i = static_cast<int>(m_decks.size());
            i < m_pPlayerManager->numberOfDecks();
            ++i) {
        auto observer = std::make_unique<DeckObserver>();
        observer->pPlayer = m_pPlayerManager->getDeckBase(i);
        const QString group = observer->pPlayer ? observer->pPlayer->getGroup()
                                                : PlayerManager::groupForDeck(i);
        observer->pPlay = std::make_unique<ControlProxy>(group, QStringLiteral("play"));
        observer->pPosition =
                std::make_unique<ControlProxy>(group, QStringLiteral("playposition"));
        // Engine controls may be set outside the GUI thread. Always hop to the
        // adapter's thread before reading models or serializing semantic state.
        observer->pPlay->connectValueChanged(
                this,
                [this](double) {
                    publish(false);
                },
                Qt::QueuedConnection);
        if (observer->pPlayer) {
            connect(observer->pPlayer,
                    &BaseTrackPlayer::newTrackLoaded,
                    this,
                    [this](const TrackPointer&) {
                        publish(false);
                    },
                    Qt::QueuedConnection);
            connect(observer->pPlayer,
                    &BaseTrackPlayer::trackUnloaded,
                    this,
                    [this](const TrackPointer&) {
                        publish(false);
                    },
                    Qt::QueuedConnection);
        }
        m_decks.push_back(std::move(observer));
    }
}

Track TangoQAdapter::trackState(const TrackPointer& pTrack) const {
    if (!pTrack) {
        return {};
    }
    const TrackId trackId = pTrack->getId();
    const double durationSeconds = pTrack->getDuration();
    const std::optional<qint64> durationMs =
            std::isfinite(durationSeconds) && durationSeconds > 0.0
            ? std::optional<qint64>(qRound64(durationSeconds * 1000.0))
            : std::nullopt;
    return {
            trackId.isValid()
                    ? QStringLiteral("tangoq:") + trackId.toString()
                    : QString(),
            pTrack->getArtist(),
            pTrack->getTitle(),
            durationMs,
    };
}

void TangoQAdapter::rebuildQueueCache() {
    m_queue.clear();
    auto* pModel = m_pAutoDJProcessor->getTableModel();
    m_queue.reserve(pModel->rowCount());
    for (int row = 0; row < pModel->rowCount(); ++row) {
        const TrackPointer pTrack = pModel->getTrack(pModel->index(row, 0));
        m_queue.append({row + 1, trackState(pTrack)});
    }
}

void TangoQAdapter::publish(bool rebuildQueue) {
    if (rebuildQueue) {
        rebuildQueueCache();
    }
    m_pStore->publish(projectState(buildProjectionInput()));
}

ProjectionInput TangoQAdapter::buildProjectionInput() const {
    const int activePosition = m_pAutoDJProcessor->activeKeepQueuePosition();
    ProjectionInput input;
    input.sessionId = m_sessionId;
    input.startedAt = m_startedAt;
    input.queue = m_queue;
    if (activePosition > 0 && activePosition <= m_queue.size()) {
        input.activeQueuePosition = activePosition;
    }

    for (const auto& span : m_pTandaQueueState->spans()) {
        input.tandas.append({
                span.id.toString(QUuid::WithoutBraces),
                tandaTypeToProtocolString(span.type),
                span.name,
                span.anchorPosition,
                static_cast<int>(span.members.size()),
        });
    }

    const auto& queueIds = m_pTandaQueueState->queueSnapshot();
    const int sharedSize = std::min(queueIds.size(), m_queue.size());
    for (int index = 0; index < sharedSize; ++index) {
        if (CortinaRegistry::instance().contains(queueIds.at(index))) {
            input.cortinaTrackIds.insert(m_queue.at(index).track.id);
        }
    }

    input.decks.reserve(static_cast<int>(m_decks.size()));
    for (const auto& pDeck : m_decks) {
        DeckState deck;
        if (pDeck->pPlayer) {
            deck.track = trackState(pDeck->pPlayer->getLoadedTrack());
            if (deck.track->id.isEmpty()) {
                deck.track.reset();
            }
            deck.playing = pDeck->pPlay->toBool();
            if (deck.track && deck.track->durationMs) {
                deck.positionMs = std::clamp(
                        qRound64(pDeck->pPosition->get() * *deck.track->durationMs),
                        qint64{0},
                        *deck.track->durationMs);
            }
        }
        input.decks.append(std::move(deck));
    }
    return input;
}

} // namespace mixxx::semanticstate
