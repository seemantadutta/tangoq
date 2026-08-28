// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#include "semanticstate/tangoqsemanticstateadapter.h"

#include <QPointer>
#include <QUuid>
#include <algorithm>

#include "control/controlproxy.h"
#include "library/autodj/autodjprocessor.h"
#include "library/autodj/cortinaregistry.h"
#include "library/autodj/tandaqueuestate.h"
#include "library/dao/playlistdao.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playermanager.h"
#include "semanticstate/semanticstatestore.h"
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
          m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces)),
          m_startedAt(QDateTime::currentDateTimeUtc()) {
    for (int i = 0; i < pPlayerManager->numberOfDecks(); ++i) {
        auto observer = std::make_unique<DeckObserver>();
        observer->pPlayer = pPlayerManager->getDeckBase(i);
        const QString group = PlayerManager::groupForDeck(i);
        observer->pPlay = std::make_unique<ControlProxy>(group, QStringLiteral("play"));
        observer->pPosition =
                std::make_unique<ControlProxy>(group, QStringLiteral("playposition"));
        observer->pPlay->connectValueChanged(this, [this](double) {
            publish(QStringLiteral("playback.stateChanged"), false);
        });
        if (observer->pPlayer) {
            connect(observer->pPlayer,
                    &BaseTrackPlayer::newTrackLoaded,
                    this,
                    [this](const TrackPointer&) {
                        publish(QStringLiteral("playback.trackLoaded"), false);
                    });
            connect(observer->pPlayer,
                    &BaseTrackPlayer::trackUnloaded,
                    this,
                    [this](const TrackPointer&) {
                        publish(QStringLiteral("playback.trackUnloaded"), false);
                    });
        }
        m_decks.push_back(std::move(observer));
    }

    const auto queueAdded = [this](const QSet<int>& playlistIds) {
        if (affectsPlaylist(playlistIds, m_autoDJPlaylistId)) {
            publish(QStringLiteral("queue.tracksAdded"), true);
        }
    };
    const auto queueMoved = [this](const QSet<int>& playlistIds) {
        if (affectsPlaylist(playlistIds, m_autoDJPlaylistId)) {
            publish(QStringLiteral("queue.tracksReordered"), true);
        }
    };
    const auto queueRemoved = [this](const QSet<int>& playlistIds) {
        if (affectsPlaylist(playlistIds, m_autoDJPlaylistId)) {
            publish(QStringLiteral("queue.tracksRemoved"), true);
        }
    };
    connect(pPlaylistDao, &PlaylistDAO::tracksAdded, this, queueAdded);
    connect(pPlaylistDao, &PlaylistDAO::tracksMoved, this, queueMoved);
    connect(pPlaylistDao, &PlaylistDAO::tracksRemoved, this, queueRemoved);
    connect(
            m_pTandaQueueState,
            &TandaQueueState::spansChanged,
            this,
            [this]() {
                publish(QStringLiteral("extensions.tangoq.tandasChanged"),
                        false);
            },
            Qt::QueuedConnection);
    connect(&CortinaRegistry::instance(),
            &CortinaRegistry::cortinaMarksChanged,
            this,
            [this]() {
                publish(QStringLiteral("extensions.tangoq.cortinasChanged"), false);
            });
    connect(m_pAutoDJProcessor,
            &AutoDJProcessor::autoDJStateChanged,
            this,
            [this](AutoDJProcessor::AutoDJState) {
                publish(QStringLiteral("playback.stateChanged"), false);
            });

    m_positionTimer.setInterval(kPositionPublishIntervalMs);
    m_positionTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_positionTimer, &QTimer::timeout, this, [this]() {
        publish(QStringLiteral("playback.positionChanged"), false);
    });
    m_positionTimer.start();

    rebuildQueueCache();
    m_pStore->publish(buildState(), QStringLiteral("session.started"));
}

TangoQAdapter::~TangoQAdapter() = default;

Track TangoQAdapter::trackState(const TrackPointer& pTrack) const {
    if (!pTrack) {
        return {};
    }
    return {
            QStringLiteral("tangoq:") + pTrack->getId().toString(),
            pTrack->getArtist(),
            pTrack->getTitle(),
            std::max<qint64>(0, qRound64(pTrack->getDuration() * 1000.0)),
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

void TangoQAdapter::publish(const QString& changeType, bool rebuildQueue) {
    if (rebuildQueue) {
        rebuildQueueCache();
    }
    m_pStore->publish(buildState(), changeType);
}

State TangoQAdapter::buildState() {
    const int activePosition = m_pAutoDJProcessor->activeKeepQueuePosition();
    return {
            m_sessionId,
            m_startedAt,
            buildPlayback(activePosition),
            m_queue,
            buildTangoQExtension(activePosition),
    };
}

Playback TangoQAdapter::buildPlayback(int activePosition) const {
    TrackId activeTrackId;
    if (activePosition > 0 && activePosition <= m_pTandaQueueState->queueSnapshot().size()) {
        activeTrackId = m_pTandaQueueState->queueSnapshot().at(activePosition - 1);
    }

    const DeckObserver* pSelected = nullptr;
    for (const auto& pDeck : m_decks) {
        if (!pDeck->pPlayer) {
            continue;
        }
        const TrackPointer pLoaded = pDeck->pPlayer->getLoadedTrack();
        if (pLoaded && activeTrackId.isValid() && pLoaded->getId() == activeTrackId &&
                pDeck->pPlay->toBool()) {
            pSelected = pDeck.get();
            break;
        }
    }
    if (!pSelected) {
        for (const auto& pDeck : m_decks) {
            if (pDeck->pPlayer && pDeck->pPlay->toBool() &&
                    pDeck->pPlayer->getLoadedTrack()) {
                pSelected = pDeck.get();
                break;
            }
        }
    }
    if (!pSelected && activeTrackId.isValid()) {
        for (const auto& pDeck : m_decks) {
            const TrackPointer pLoaded =
                    pDeck->pPlayer ? pDeck->pPlayer->getLoadedTrack() : TrackPointer{};
            if (pLoaded && pLoaded->getId() == activeTrackId) {
                pSelected = pDeck.get();
                break;
            }
        }
    }

    Playback playback;
    playback.queuePosition = activePosition;
    if (!pSelected || !pSelected->pPlayer) {
        return playback;
    }

    const TrackPointer pTrack = pSelected->pPlayer->getLoadedTrack();
    playback.track = trackState(pTrack);
    playback.durationMs = playback.track->durationMs;
    playback.positionMs = std::clamp(
            qRound64(pSelected->pPosition->get() * playback.durationMs),
            qint64{0},
            playback.durationMs);
    if (pSelected->pPlay->toBool()) {
        playback.state = QStringLiteral("playing");
    } else if (m_pAutoDJProcessor->getState() != AutoDJProcessor::ADJ_DISABLED) {
        playback.state = QStringLiteral("paused");
    }
    return playback;
}

TangoQExtension TangoQAdapter::buildTangoQExtension(int activePosition) const {
    TangoQExtension extension;
    for (const auto& span : m_pTandaQueueState->spans()) {
        Tanda tanda{
                span.id.toString(QUuid::WithoutBraces),
                tandaTypeToProtocolString(span.type),
                span.name,
                span.anchorPosition,
                static_cast<int>(span.members.size()),
                0,
        };
        extension.tandas.append(tanda);
        const int endPosition = span.anchorPosition + span.members.size() - 1;
        if (activePosition >= span.anchorPosition && activePosition <= endPosition) {
            tanda.trackIndex = activePosition - span.anchorPosition + 1;
            extension.currentTanda = tanda;
        } else if (activePosition > 0 && span.anchorPosition > activePosition &&
                !extension.upcomingTanda) {
            extension.upcomingTanda = tanda;
        }
    }
    if (activePosition == 0 && !extension.tandas.isEmpty()) {
        extension.upcomingTanda = extension.tandas.first();
    }

    const auto& queueIds = m_pTandaQueueState->queueSnapshot();
    if (activePosition > 0 && activePosition <= queueIds.size() &&
            CortinaRegistry::instance().contains(queueIds.at(activePosition - 1))) {
        extension.cortina.state = QStringLiteral("current");
        extension.cortina.queuePosition = activePosition;
        if (activePosition <= m_queue.size()) {
            extension.cortina.track = m_queue.at(activePosition - 1).track;
        }
    } else {
        const int firstCandidate = std::max(1, activePosition + 1);
        for (int position = firstCandidate; position <= queueIds.size(); ++position) {
            if (CortinaRegistry::instance().contains(queueIds.at(position - 1))) {
                extension.cortina.state = QStringLiteral("upcoming");
                extension.cortina.queuePosition = position;
                if (position <= m_queue.size()) {
                    extension.cortina.track = m_queue.at(position - 1).track;
                }
                break;
            }
        }
    }
    return extension;
}

} // namespace mixxx::semanticstate
