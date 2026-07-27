#include "library/autodj/tracktyperegistry.h"

#include <QObject>

#include "moc_tracktyperegistry.cpp"

// static
TrackTypeRegistry& TrackTypeRegistry::instance() {
    static TrackTypeRegistry s_instance;
    return s_instance;
}

void TrackTypeRegistry::setType(TrackId trackId, TangoTrackType type) {
    if (!trackId.isValid()) {
        return;
    }
    if (type == TangoTrackType::Milonga) {
        // The default is stored as absence, so an untyped queue stays empty.
        if (m_types.remove(trackId) > 0) {
            emit trackTypesChanged();
        }
        return;
    }
    if (m_types.value(trackId, TangoTrackType::Milonga) == type) {
        return;
    }
    m_types.insert(trackId, type);
    emit trackTypesChanged();
}

// static
QString TrackTypeRegistry::tagFor(TangoTrackType type) {
    switch (type) {
    case TangoTrackType::Intro:
        return QObject::tr("INTRO");
    case TangoTrackType::Outro:
        return QObject::tr("OUTRO");
    case TangoTrackType::Performance:
        return QObject::tr("PERFORMANCE");
    case TangoTrackType::Milonga:
        break;
    }
    // Milonga is the default and carries no tag: the untyped majority of a queue
    // should read as plainly as it does in stock Mixxx.
    return QString();
}
