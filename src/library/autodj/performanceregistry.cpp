// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/performanceregistry.h"

#include "moc_performanceregistry.cpp"

// static
PerformanceRegistry& PerformanceRegistry::instance() {
    static PerformanceRegistry s_instance;
    return s_instance;
}

void PerformanceRegistry::mark(TrackId trackId) {
    if (!trackId.isValid() || m_trackIds.contains(trackId)) {
        return;
    }
    m_trackIds.insert(trackId);
    emit performanceMarksChanged();
}

void PerformanceRegistry::unmark(TrackId trackId) {
    if (m_trackIds.remove(trackId)) {
        emit performanceMarksChanged();
    }
}
