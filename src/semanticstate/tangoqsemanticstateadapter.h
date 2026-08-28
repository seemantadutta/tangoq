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

struct ProjectionInput;
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

    void observeDecks();
    void rebuildQueueCache();
    void publish(bool rebuildQueue);
    ProjectionInput buildProjectionInput() const;
    Track trackState(const TrackPointer& pTrack) const;

    Store* const m_pStore;
    const int m_autoDJPlaylistId;
    AutoDJProcessor* const m_pAutoDJProcessor;
    TandaQueueState* const m_pTandaQueueState;
    PlayerManagerInterface* const m_pPlayerManager;
    // A monitor session is the lifetime of this publisher instance. It is not
    // an AutoDJ run, queue, tanda, or persisted milonga identity.
    const QString m_sessionId;
    const QDateTime m_startedAt;
    QVector<QueueItem> m_queue;
    std::vector<std::unique_ptr<DeckObserver>> m_decks;
    QTimer m_positionTimer;
};

} // namespace mixxx::semanticstate
