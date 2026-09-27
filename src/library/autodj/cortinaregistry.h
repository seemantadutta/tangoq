// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QObject>
#include <QSet>
#include <QSqlDatabase>

#include "track/trackid.h"

/// Registry of tracks the DJ has tagged as "cortinas" for the Auto DJ (Tango)
/// queue. A cortina is a short non-tango track played between tandas; tagged
/// tracks render with a "[--CORTINA--]" title prefix and blue text in the Auto
/// DJ list.
///
/// A mark belongs to the track: while a database is attached, marks are saved
/// in its tangoq_cortina table and loaded again on the next start, until the
/// DJ unmarks the track. Without the table (e.g. the TangoQ tables could not
/// be created) the marks last for the session only.
class CortinaRegistry : public QObject {
    Q_OBJECT
  public:
    static CortinaRegistry& instance();

    bool contains(TrackId trackId) const {
        return m_trackIds.contains(trackId);
    }

    void mark(TrackId trackId);
    void unmark(TrackId trackId);

    /// Replaces the marks in memory with the ones saved in the database, and
    /// saves every change from now on.
    void attachDatabase(const QSqlDatabase& database);
    /// Stops saving changes, e.g. before the database closes. The marks in
    /// memory are kept.
    void detachDatabase();

  signals:
    // Emitted whenever the set of tagged tracks changes, so views showing the
    // cortina styling can repaint.
    void cortinaMarksChanged();

  private:
    CortinaRegistry() = default;

    void save(TrackId trackId, bool marked);

    QSet<TrackId> m_trackIds;
    QSqlDatabase m_database;
};
