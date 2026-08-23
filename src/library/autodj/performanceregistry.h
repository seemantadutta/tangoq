// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QObject>
#include <QSet>

#include "track/trackid.h"

/// Session-only registry of tracks the DJ has tagged as "performance tracks"
/// for the Auto DJ (Tango) queue. A performance track is a one-off (a show or a
/// special request) that sits outside the tanda structure; tagged tracks show a
/// green "P" in the Auto DJ list's Item Type column. Like cortina marks these
/// are intentionally NOT persisted: they live for the current Mixxx session
/// only and are cleared on restart. A track is a cortina or a performance track
/// but never both - the two registries are kept mutually exclusive by the menu.
class PerformanceRegistry : public QObject {
    Q_OBJECT
  public:
    static PerformanceRegistry& instance();

    bool contains(TrackId trackId) const {
        return m_trackIds.contains(trackId);
    }

    void mark(TrackId trackId);
    void unmark(TrackId trackId);

  signals:
    // Emitted whenever the set of tagged tracks changes, so views showing the
    // performance styling can repaint.
    void performanceMarksChanged();

  private:
    PerformanceRegistry() = default;

    QSet<TrackId> m_trackIds;
};
