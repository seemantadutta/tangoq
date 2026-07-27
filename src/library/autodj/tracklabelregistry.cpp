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
        if (m_labels.remove(trackId) > 0) {
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
