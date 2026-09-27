// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QUuid>
#include <QVector>

/// Checks that the TangoQ queue reads like a milonga set: tandas separated by
/// cortinas. Cortina marks belong to the track and are remembered, so a track
/// marked by mistake would otherwise follow it into every future set.
namespace tandaqueuechecks {

enum class Problem {
    // A cortina is grouped into a tanda.
    CortinaInsideTanda,
    // A cortina's genre tag says Tango, Vals or Milonga.
    CortinaWithTandaGenre,
    // A cortina directly follows another cortina.
    CortinaAfterCortina,
    // A tanda directly follows another tanda, with no cortina between.
    NoCortinaBeforeTanda,
};

/// One queue row, in queue order.
struct Row {
    bool cortina = false;
    QString genre;
    // The tanda the row belongs to, or null.
    QUuid tandaId;
};

struct Result {
    // By 0-based queue row.
    QHash<int, QList<Problem>> rows;
    // By tanda, for its header: a problem with the tanda itself, or with a
    // row inside it (so a collapsed tanda still shows it).
    QHash<QUuid, QList<Problem>> tandas;

    bool isEmpty() const {
        return rows.isEmpty() && tandas.isEmpty();
    }
};

Result check(const QVector<Row>& rows);

/// Whether a genre tag names a tanda genre: Tango, Vals or Milonga as a word,
/// in any case (e.g. "tango", "Vals criollo").
bool isTandaGenre(const QString& genre);

} // namespace tandaqueuechecks
