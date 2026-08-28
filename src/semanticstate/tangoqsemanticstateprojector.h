// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#pragma once

#include <QSet>

#include "semanticstate/semanticstatemodel.h"

namespace mixxx::semanticstate {

struct DeckState {
    std::optional<Track> track;
    bool playing{false};
    std::optional<qint64> positionMs;
};

struct ProjectionInput {
    QString sessionId;
    QDateTime startedAt;
    QVector<QueueItem> queue;
    QVector<Tanda> tandas;
    QSet<QString> cortinaTrackIds;
    QVector<DeckState> decks;
    // One-based AutoDJ cursor position, or null when AutoDJ has no active item.
    std::optional<int> activeQueuePosition;
};

/// Converts a point-in-time view of authoritative TangoQ state into the public
/// semantic contract. This function contains no model, network, or thread access.
State projectState(const ProjectionInput& input);

} // namespace mixxx::semanticstate
