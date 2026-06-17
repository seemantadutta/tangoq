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
