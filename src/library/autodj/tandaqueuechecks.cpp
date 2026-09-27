// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/tandaqueuechecks.h"

#include <QRegularExpression>

namespace tandaqueuechecks {

bool isTandaGenre(const QString& genre) {
    static const QRegularExpression kTandaGenre(
            QStringLiteral("\\b(tango|vals|milonga)\\b"),
            QRegularExpression::CaseInsensitiveOption);
    return !genre.isEmpty() && kTandaGenre.match(genre).hasMatch();
}

Result check(const QVector<Row>& rows) {
    Result result;
    for (int i = 0; i < rows.size(); ++i) {
        const Row& row = rows.at(i);
        const Row* pPrevious = i > 0 ? &rows.at(i - 1) : nullptr;
        if (row.cortina) {
            if (!row.tandaId.isNull()) {
                result.rows[i].append(Problem::CortinaInsideTanda);
                if (!result.tandas.value(row.tandaId).contains(Problem::CortinaInsideTanda)) {
                    result.tandas[row.tandaId].append(Problem::CortinaInsideTanda);
                }
            }
            if (isTandaGenre(row.genre)) {
                result.rows[i].append(Problem::CortinaWithTandaGenre);
            }
            if (pPrevious && pPrevious->cortina) {
                result.rows[i].append(Problem::CortinaAfterCortina);
            }
        }
        // The first row of a tanda that directly follows a row of another one.
        if (!row.tandaId.isNull() && pPrevious && !pPrevious->tandaId.isNull() &&
                pPrevious->tandaId != row.tandaId) {
            result.tandas[row.tandaId].append(Problem::NoCortinaBeforeTanda);
        }
    }
    return result;
}

} // namespace tandaqueuechecks
