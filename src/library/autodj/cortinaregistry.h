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

/// Session-only registry of tracks the DJ has tagged as "cortinas" for the
/// Auto DJ (Tango) queue. A cortina is a short non-tango track played between
/// tandas; tagged tracks render with a "[--CORTINA--]" title prefix and blue
/// text in the Auto DJ list. The marks are intentionally NOT persisted: they
/// live for the current Mixxx session only and are cleared on restart.
class CortinaRegistry : public QObject {
    Q_OBJECT
  public:
    static CortinaRegistry& instance();

    bool contains(TrackId trackId) const {
        return m_trackIds.contains(trackId);
    }

    void mark(TrackId trackId);
    void unmark(TrackId trackId);

  signals:
    // Emitted whenever the set of tagged tracks changes, so views showing the
    // cortina styling can repaint.
    void cortinaMarksChanged();

  private:
    CortinaRegistry() = default;

    QSet<TrackId> m_trackIds;
};
