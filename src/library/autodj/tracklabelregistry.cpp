// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/tracklabelregistry.h"

#include "moc_tracklabelregistry.cpp"

// static
TrackLabelRegistry& TrackLabelRegistry::instance() {
    static TrackLabelRegistry s_instance;
    return s_instance;
}

void TrackLabelRegistry::setLabel(TrackId trackId, const QString& label) {
    if (!trackId.isValid()) {
        return;
    }
    const QString trimmed = label.trimmed();
    if (trimmed.isEmpty()) {
        // Clearing rather than storing an empty label, so a row with nothing to
        // say falls back to its real title instead of showing a blank.
        // QHash::remove() returns bool in Qt 6 (it returned a count in Qt 5), so
        // test it directly - comparing it against 0 is what MSVC flags as C4804.
        if (m_labels.remove(trackId)) {
            emit trackLabelsChanged();
        }
        return;
    }
    if (m_labels.value(trackId) == trimmed) {
        return;
    }
    m_labels.insert(trackId, trimmed);
    emit trackLabelsChanged();
}
