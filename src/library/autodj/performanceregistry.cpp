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
