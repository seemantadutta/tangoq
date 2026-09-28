// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QHash>
#include <QMetaType>
#include <QSqlDatabase>
#include <QString>
#include <QUuid>

#include "track/track_decl.h"

class TandaQueueState;

/// What a track was when TangoQ played it. Stored as an integer in the
/// tangoq_play table, so never renumber these.
enum class TangoPlayRole {
    Regular = 0,
    Cortina = 1,
    Performance = 2,
};

/// A play TangoQ logged to history, as it happened: the track's role at the
/// time, whether LIVE mode was on, and the tanda it was part of. Saved in the
/// tangoq_play table next to its history row, so the history shows it as it was
/// played even after the marks or tandas change.
struct TangoPlay {
    TrackPointer track; // Only set for a play being logged, not a loaded one.
    TangoPlayRole role = TangoPlayRole::Regular;
    bool live = false;
    // The tanda, if the track was part of one. tandaType is a TandaType value.
    QUuid tandaId;
    int tandaType = -1;
    QString tandaName;
    int tandaPosition = 0; // 1-based, out of tandaSize
    int tandaSize = 0;

    bool inTanda() const {
        return !tandaId.isNull();
    }
};

Q_DECLARE_METATYPE(TangoPlay)

namespace tangoplay {

/// Describes the play of the track TangoQ started from oneBasedQueuePosition,
/// from the current cortina and performance marks and the tanda at that
/// position. pTandaState may be null.
TangoPlay describe(const TrackPointer& pTrack,
        int oneBasedQueuePosition,
        const TandaQueueState* pTandaState,
        bool live);

/// Saves the play next to its history row, which is the most recent row for
/// its track in the history session. Returns false if that row is missing or
/// the table cannot be written (e.g. the TangoQ tables do not exist).
bool save(const QSqlDatabase& database, int playlistId, const TangoPlay& play);

/// The saved plays of a history playlist, by 1-based position in it. A play
/// is matched to its row by track and time, which a row keeps when "Join with
/// previous" copies it to another session. Rows without a saved play (e.g.
/// recorded before TangoQ saved plays) are missing.
QHash<int, TangoPlay> load(const QSqlDatabase& database, int playlistId);

} // namespace tangoplay
