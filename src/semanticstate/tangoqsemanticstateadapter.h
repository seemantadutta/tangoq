// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#pragma once

#include <QObject>
#include <QTimer>
#include <memory>
#include <vector>

#include "semanticstate/semanticstatemodel.h"
#include "track/track_decl.h"

class AutoDJProcessor;
class PlayerManagerInterface;
class PlaylistDAO;
class TandaQueueState;

namespace mixxx::semanticstate {

class Store;

class TangoQAdapter final : public QObject {
  public:
    TangoQAdapter(Store* pStore,
            PlaylistDAO* pPlaylistDao,
            int autoDJPlaylistId,
            AutoDJProcessor* pAutoDJProcessor,
            TandaQueueState* pTandaQueueState,
            PlayerManagerInterface* pPlayerManager,
            QObject* pParent = nullptr);
    ~TangoQAdapter() override;

  private:
    struct DeckObserver;

    void rebuildQueueCache();
    void publish(const QString& changeType, bool rebuildQueue);
    State buildState();
    Playback buildPlayback(int activePosition) const;
    TangoQExtension buildTangoQExtension(int activePosition) const;
    Track trackState(const TrackPointer& pTrack) const;

    Store* const m_pStore;
    const int m_autoDJPlaylistId;
    AutoDJProcessor* const m_pAutoDJProcessor;
    TandaQueueState* const m_pTandaQueueState;
    const QString m_sessionId;
    const QDateTime m_startedAt;
    QVector<QueueItem> m_queue;
    std::vector<std::unique_ptr<DeckObserver>> m_decks;
    QTimer m_positionTimer;
};

} // namespace mixxx::semanticstate
