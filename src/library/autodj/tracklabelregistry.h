// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QHash>
#include <QObject>
#include <QString>

#include "track/trackid.h"

/// Session-only display names for tracks in the Auto DJ (Tango) queue. A set
/// often contains tracks whose real title says nothing about the job they do in
/// the night - a performance piece, an intro, an outro - and the DJ needs to
/// pick those out of a long queue at a glance.
///
/// The label replaces the title *in the Auto DJ list only*. The stored metadata
/// is never touched, and the other columns keep showing the real artist and
/// album, so a labelled row is still identifiable as the file it is.
///
/// Like the cortina marks these are deliberately NOT persisted: they describe a
/// track's role in tonight's set, not a property of the file, and they are
/// cleared on restart.
class TrackLabelRegistry : public QObject {
    Q_OBJECT
  public:
    static TrackLabelRegistry& instance();

    bool contains(TrackId trackId) const {
        return m_labels.contains(trackId);
    }

    QString label(TrackId trackId) const {
        return m_labels.value(trackId);
    }

    /// An empty label clears the entry rather than displaying nothing.
    void setLabel(TrackId trackId, const QString& label);

  signals:
    /// Emitted whenever a label is set or cleared, so views repaint.
    void trackLabelsChanged();

  private:
    TrackLabelRegistry() = default;

    QHash<TrackId, QString> m_labels;
};
