// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/cortinaregistry.h"

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
    emit cortinaMarksChanged();
}

void CortinaRegistry::unmark(TrackId trackId) {
    if (m_trackIds.remove(trackId)) {
        emit cortinaMarksChanged();
    }
}
