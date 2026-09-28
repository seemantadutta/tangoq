// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/tangoplay.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "library/autodj/cortinaregistry.h"
#include "library/autodj/performanceregistry.h"
#include "library/autodj/tandaqueuestate.h"
#include "track/track.h"

namespace tangoplay {

TangoPlay describe(const TrackPointer& pTrack,
        int oneBasedQueuePosition,
        const TandaQueueState* pTandaState,
        bool live) {
    TangoPlay play;
    play.track = pTrack;
    play.live = live;
    if (!pTrack) {
        return play;
    }
    const TrackId trackId = pTrack->getId();
    if (CortinaRegistry::instance().contains(trackId)) {
        play.role = TangoPlayRole::Cortina;
    } else if (PerformanceRegistry::instance().contains(trackId)) {
        play.role = TangoPlayRole::Performance;
    }
    const TandaSpan* pSpan = pTandaState
            ? pTandaState->spanAtPosition(oneBasedQueuePosition)
            : nullptr;
    if (pSpan) {
        play.tandaId = pSpan->id;
        play.tandaType = static_cast<int>(pSpan->type);
        play.tandaName = pSpan->name;
        play.tandaPosition = oneBasedQueuePosition - pSpan->anchorPosition + 1;
        play.tandaSize = static_cast<int>(pSpan->members.size());
    }
    return play;
}

bool save(const QSqlDatabase& database, int playlistId, const TangoPlay& play) {
    if (!play.track) {
        return false;
    }
    const QVariant trackId = play.track->getId().toVariant();
    // The history row just appended for this play: the track's last row in
    // the session. Its timestamp is what links the play to the row.
    QSqlQuery row(database);
    row.prepare(QStringLiteral(
            "SELECT pl_datetime_added FROM PlaylistTracks "
            "WHERE playlist_id = :playlist_id AND track_id = :track_id "
            "ORDER BY position DESC LIMIT 1"));
    row.bindValue(QStringLiteral(":playlist_id"), playlistId);
    row.bindValue(QStringLiteral(":track_id"), trackId);
    if (!row.exec() || !row.next()) {
        qWarning() << "No history row to save the TangoQ play for track"
                   << trackId << row.lastError().text();
        return false;
    }
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
            "INSERT INTO tangoq_play (playlist_id, track_id, played_at, role, "
            "live, tanda_id, tanda_type, tanda_name, tanda_position, tanda_size) "
            "VALUES (:playlist_id, :track_id, :played_at, :role, :live, "
            ":tanda_id, :tanda_type, :tanda_name, :tanda_position, :tanda_size)"));
    insert.bindValue(QStringLiteral(":playlist_id"), playlistId);
    insert.bindValue(QStringLiteral(":track_id"), trackId);
    insert.bindValue(QStringLiteral(":played_at"), row.value(0));
    insert.bindValue(QStringLiteral(":role"), static_cast<int>(play.role));
    insert.bindValue(QStringLiteral(":live"), play.live ? 1 : 0);
    if (play.inTanda()) {
        insert.bindValue(QStringLiteral(":tanda_id"),
                play.tandaId.toString(QUuid::WithoutBraces));
        insert.bindValue(QStringLiteral(":tanda_type"), play.tandaType);
        insert.bindValue(QStringLiteral(":tanda_name"), play.tandaName);
        insert.bindValue(QStringLiteral(":tanda_position"), play.tandaPosition);
        insert.bindValue(QStringLiteral(":tanda_size"), play.tandaSize);
    } else {
        const QVariant null;
        insert.bindValue(QStringLiteral(":tanda_id"), null);
        insert.bindValue(QStringLiteral(":tanda_type"), null);
        insert.bindValue(QStringLiteral(":tanda_name"), null);
        insert.bindValue(QStringLiteral(":tanda_position"), null);
        insert.bindValue(QStringLiteral(":tanda_size"), null);
    }
    if (!insert.exec()) {
        qWarning() << "Could not save the TangoQ play for track" << trackId
                   << insert.lastError().text();
        return false;
    }
    return true;
}

QHash<int, TangoPlay> load(const QSqlDatabase& database, int playlistId) {
    QHash<int, TangoPlay> plays;
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
            "SELECT PlaylistTracks.position, tangoq_play.role, tangoq_play.live, "
            "tangoq_play.tanda_id, tangoq_play.tanda_type, tangoq_play.tanda_name, "
            "tangoq_play.tanda_position, tangoq_play.tanda_size "
            "FROM PlaylistTracks INNER JOIN tangoq_play "
            "ON tangoq_play.track_id = PlaylistTracks.track_id "
            "AND tangoq_play.played_at = PlaylistTracks.pl_datetime_added "
            "WHERE PlaylistTracks.playlist_id = :playlist_id"));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    if (!query.exec()) {
        // E.g. the TangoQ tables do not exist: the history shows no roles.
        return plays;
    }
    while (query.next()) {
        TangoPlay play;
        play.role = static_cast<TangoPlayRole>(query.value(1).toInt());
        play.live = query.value(2).toBool();
        if (!query.value(3).isNull()) {
            play.tandaId = QUuid(query.value(3).toString());
            play.tandaType = query.value(4).toInt();
            play.tandaName = query.value(5).toString();
            play.tandaPosition = query.value(6).toInt();
            play.tandaSize = query.value(7).toInt();
        }
        plays.insert(query.value(0).toInt(), play);
    }
    return plays;
}

} // namespace tangoplay
