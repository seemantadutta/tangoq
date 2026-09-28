// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/cortinaregistry.h"

#include <QSqlError>
#include <QSqlQuery>

#include "moc_cortinaregistry.cpp"

// static
CortinaRegistry& CortinaRegistry::instance() {
    static CortinaRegistry s_instance;
    return s_instance;
}

void CortinaRegistry::mark(TrackId trackId) {
    if (!trackId.isValid() || m_trackIds.contains(trackId)) {
        return;
    }
    m_trackIds.insert(trackId);
    save(trackId, true);
    emit cortinaMarksChanged();
}

void CortinaRegistry::unmark(TrackId trackId) {
    if (m_trackIds.remove(trackId)) {
        save(trackId, false);
        emit cortinaMarksChanged();
    }
}

void CortinaRegistry::attachDatabase(const QSqlDatabase& database) {
    m_database = database;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT track_id FROM tangoq_cortina"))) {
        qWarning() << "Cortina marks will not be saved:" << query.lastError().text();
        m_database = QSqlDatabase();
        return;
    }
    QSet<TrackId> trackIds;
    while (query.next()) {
        const TrackId trackId(query.value(0));
        if (trackId.isValid()) {
            trackIds.insert(trackId);
        }
    }
    m_trackIds = std::move(trackIds);
    emit cortinaMarksChanged();
}

void CortinaRegistry::detachDatabase() {
    m_database = QSqlDatabase();
}

void CortinaRegistry::save(TrackId trackId, bool marked) {
    if (!m_database.isValid()) {
        return;
    }
    QSqlQuery query(m_database);
    query.prepare(marked
                    ? QStringLiteral(
                              "INSERT OR IGNORE INTO tangoq_cortina (track_id) VALUES (:id)")
                    : QStringLiteral("DELETE FROM tangoq_cortina WHERE track_id = :id"));
    query.bindValue(QStringLiteral(":id"), trackId.toVariant());
    if (!query.exec()) {
        // The mark still applies for this session.
        qWarning() << "Could not save the cortina mark for track" << trackId
                   << query.lastError().text();
    }
}
