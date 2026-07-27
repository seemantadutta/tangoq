#pragma once

#include <QHash>
#include <QObject>

#include "track/trackid.h"

/// What job a track does in the night. A milonga is not one flat list of dance
/// music: a sound check runs before it, performances interrupt it, and filler
/// plays afterwards while people say their goodbyes. Those blocks behave
/// differently enough that Auto DJ has to know which is which.
///
///  - Milonga:     social dancing. The default, and what plain Mixxx does.
///  - Intro:       before the milonga, sound check. Outside milonga time.
///  - Outro:       after it, while people pack up. Outside milonga time.
///  - Performance: plays in full and always stops afterwards, so the audience
///                 can applaud and the organiser can speak.
///
/// Transitions run normally within a run of the same type; a change of type
/// stops the set, which is what keeps a sound check from sliding into the
/// milonga and the milonga from sliding into the goodbyes.
enum class TangoTrackType {
    Milonga,
    Intro,
    Outro,
    Performance,
};

/// Session-only track types for the Auto DJ (Tango) queue.
///
/// Milonga is the default and is stored as absence, so an untyped queue behaves
/// exactly like stock Mixxx and nothing has to be migrated.
///
/// Like the cortina marks and display names these are deliberately NOT
/// persisted: a type describes a track's role in tonight's set rather than a
/// property of the recording, and the same file can be a performance one night
/// and a tanda track the next.
class TrackTypeRegistry : public QObject {
    Q_OBJECT
  public:
    static TrackTypeRegistry& instance();

    TangoTrackType type(TrackId trackId) const {
        return m_types.value(trackId, TangoTrackType::Milonga);
    }

    /// Setting Milonga clears the entry rather than storing the default.
    void setType(TrackId trackId, TangoTrackType type);

    /// Display name for a type, or empty for Milonga, which needs no tag.
    static QString tagFor(TangoTrackType type);

  signals:
    /// Emitted whenever a type changes, so views repaint and Auto DJ recomputes
    /// the set length.
    void trackTypesChanged();

  private:
    TrackTypeRegistry() = default;

    QHash<TrackId, TangoTrackType> m_types;
};
