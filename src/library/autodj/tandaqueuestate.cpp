// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/tandaqueuestate.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtDebug>
#include <algorithm>

#include "moc_tandaqueuestate.cpp"

namespace {

const ConfigKey kTandaStateConfigKey(
        QStringLiteral("[TangoMode]"), QStringLiteral("AutoDjTandasV1"));
constexpr int kSerializationVersion = 1;

void setError(QString* pError, const QString& error) {
    if (pError) {
        *pError = error;
    }
}

} // namespace

TandaQueueState::TandaQueueState(UserSettingsPointer pConfig, QObject* pParent)
        : QObject(pParent),
          m_pConfig(std::move(pConfig)) {
}

QString TandaQueueState::typeToString(TandaType type) {
    switch (type) {
    case TandaType::Tango:
        return QStringLiteral("tango");
    case TandaType::Vals:
        return QStringLiteral("vals");
    case TandaType::Milonga:
        return QStringLiteral("milonga");
    case TandaType::NuevoAlternative:
        return QStringLiteral("nuevoAlternative");
    }
    DEBUG_ASSERT(false);
    return QStringLiteral("tango");
}

bool TandaQueueState::typeFromString(const QString& value, TandaType* pType) {
    if (value == QStringLiteral("tango")) {
        *pType = TandaType::Tango;
    } else if (value == QStringLiteral("vals")) {
        *pType = TandaType::Vals;
    } else if (value == QStringLiteral("milonga")) {
        *pType = TandaType::Milonga;
    } else if (value == QStringLiteral("nuevoAlternative")) {
        *pType = TandaType::NuevoAlternative;
    } else {
        return false;
    }
    return true;
}

bool TandaQueueState::sequenceMatches(const QVector<TrackId>& queue,
        int zeroBasedStart,
        const QVector<TrackId>& members) {
    if (zeroBasedStart < 0 || members.isEmpty() ||
            zeroBasedStart + members.size() > queue.size()) {
        return false;
    }
    for (int index = 0; index < members.size(); ++index) {
        if (queue.at(zeroBasedStart + index) != members.at(index)) {
            return false;
        }
    }
    return true;
}

QVector<int> TandaQueueState::matchingStarts(
        const QVector<TrackId>& queue, const QVector<TrackId>& members) {
    QVector<int> matches;
    for (int start = 0; start + members.size() <= queue.size(); ++start) {
        if (sequenceMatches(queue, start, members)) {
            matches.append(start);
        }
    }
    return matches;
}

bool TandaQueueState::rangesOverlap(int firstStart,
        int firstLength,
        int secondStart,
        int secondLength) {
    return firstStart < secondStart + secondLength &&
            secondStart < firstStart + firstLength;
}

void TandaQueueState::restore(const QVector<TrackId>& queue) {
    m_queueSnapshot = queue;
    m_spans.clear();

    if (!m_pConfig) {
        emit spansChanged();
        return;
    }

    const QString serialized =
            m_pConfig->getValue(kTandaStateConfigKey, QString());
    if (serialized.isEmpty()) {
        emit spansChanged();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
            QJsonDocument::fromJson(serialized.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            document.object().value(QStringLiteral("version")).toInt() !=
                    kSerializationVersion) {
        qWarning() << "Discarding malformed Auto DJ tanda state:" << parseError.errorString();
        save();
        emit spansChanged();
        return;
    }

    const QJsonArray serializedSpans =
            document.object().value(QStringLiteral("spans")).toArray();
    for (const QJsonValue& value : serializedSpans) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        TandaSpan span;
        span.id = QUuid(object.value(QStringLiteral("id")).toString());
        span.name = object.value(QStringLiteral("name")).toString();
        span.anchorPosition = object.value(QStringLiteral("anchor")).toInt();
        span.collapsed = object.value(QStringLiteral("collapsed")).toBool();
        if (span.id.isNull() || span.anchorPosition <= 0 ||
                !typeFromString(object.value(QStringLiteral("type")).toString(),
                        &span.type)) {
            continue;
        }
        const QJsonArray members = object.value(QStringLiteral("members")).toArray();
        for (const QJsonValue& member : members) {
            const TrackId trackId(member.toVariant());
            if (!trackId.isValid()) {
                span.members.clear();
                break;
            }
            span.members.append(trackId);
        }
        if (span.members.isEmpty()) {
            continue;
        }

        int restoredStart = span.anchorPosition - 1;
        if (!sequenceMatches(queue, restoredStart, span.members)) {
            const QVector<int> matches = matchingStarts(queue, span.members);
            if (matches.size() != 1) {
                qWarning() << "Discarding ambiguous or unmatched Auto DJ tanda" << span.id;
                continue;
            }
            restoredStart = matches.first();
        }

        bool overlaps = false;
        for (const TandaSpan& accepted : std::as_const(m_spans)) {
            if (rangesOverlap(restoredStart,
                        span.members.size(),
                        accepted.anchorPosition - 1,
                        accepted.members.size())) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) {
            qWarning() << "Discarding overlapping Auto DJ tanda" << span.id;
            continue;
        }
        span.anchorPosition = restoredStart + 1;
        m_spans.append(std::move(span));
    }
    sortSpans();
    save();
    emit spansChanged();
}

void TandaQueueState::reconcileQueue(const QVector<TrackId>& queue) {
    if (queue == m_queueSnapshot) {
        m_preserveUnchangedSpansOnNextReconcile = false;
        return;
    }

    if (m_preserveUnchangedSpansOnNextReconcile) {
        m_preserveUnchangedSpansOnNextReconcile = false;
        QVector<TandaSpan> surviving;
        for (const TandaSpan& span : std::as_const(m_spans)) {
            if (sequenceMatches(queue, span.anchorPosition - 1, span.members)) {
                surviving.append(span);
            }
        }
        const bool changed = surviving.size() != m_spans.size();
        m_spans = std::move(surviving);
        m_queueSnapshot = queue;
        if (changed) {
            save();
            emit spansChanged();
        }
        return;
    }

    // The common queue operation is an append. It cannot alter membership of
    // an existing span and must not accidentally reclassify anything.
    if (queue.size() >= m_queueSnapshot.size() &&
            std::equal(m_queueSnapshot.cbegin(),
                    m_queueSnapshot.cend(),
                    queue.cbegin())) {
        m_queueSnapshot = queue;
        return;
    }

    int prefix = 0;
    while (prefix < m_queueSnapshot.size() && prefix < queue.size() &&
            m_queueSnapshot.at(prefix) == queue.at(prefix)) {
        ++prefix;
    }
    int suffix = 0;
    while (suffix < m_queueSnapshot.size() - prefix &&
            suffix < queue.size() - prefix &&
            m_queueSnapshot.at(m_queueSnapshot.size() - 1 - suffix) ==
                    queue.at(queue.size() - 1 - suffix)) {
        ++suffix;
    }
    const int oldChangeStart = prefix;
    const int oldChangeEnd = m_queueSnapshot.size() - suffix;
    const int newChangeEnd = queue.size() - suffix;
    const int sizeDelta = queue.size() - m_queueSnapshot.size();

    QVector<TandaSpan> surviving;
    for (TandaSpan span : std::as_const(m_spans)) {
        const int oldStart = span.anchorPosition - 1;
        const int oldEnd = oldStart + span.members.size();
        bool affected = false;
        if (oldChangeStart < oldChangeEnd) {
            affected = oldStart < oldChangeEnd && oldChangeStart < oldEnd;
        } else if (newChangeEnd > oldChangeStart) {
            // Pure insertion: only insertion between existing members changes
            // membership. Insertion exactly before/after merely shifts a span.
            affected = oldStart < oldChangeStart && oldChangeStart < oldEnd;
        }
        if (affected) {
            continue;
        }

        const int expectedStart = oldStart >= oldChangeEnd ? oldStart + sizeDelta : oldStart;
        if (!sequenceMatches(queue, expectedStart, span.members)) {
            continue;
        }
        span.anchorPosition = expectedStart + 1;
        surviving.append(std::move(span));
    }

    const bool changed = surviving.size() != m_spans.size() ||
            !std::equal(surviving.cbegin(),
                    surviving.cend(),
                    m_spans.cbegin(),
                    [](const TandaSpan& left, const TandaSpan& right) {
                        return left.id == right.id &&
                                left.anchorPosition == right.anchorPosition;
                    });
    m_spans = std::move(surviving);
    m_queueSnapshot = queue;
    sortSpans();
    if (changed) {
        save();
        emit spansChanged();
    }
}

QUuid TandaQueueState::classify(const QVector<int>& oneBasedPositions,
        TandaType type,
        QString* pError) {
    if (oneBasedPositions.isEmpty()) {
        setError(pError, tr("Select one or more consecutive queue tracks."));
        return {};
    }
    QVector<int> positions = oneBasedPositions;
    std::sort(positions.begin(), positions.end());
    positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
    if (positions.size() != oneBasedPositions.size() || positions.first() <= 0 ||
            positions.last() > m_queueSnapshot.size()) {
        setError(pError, tr("The selection is not a valid queue range."));
        return {};
    }
    for (int index = 1; index < positions.size(); ++index) {
        if (positions.at(index) != positions.at(index - 1) + 1) {
            setError(pError, tr("Select consecutive queue tracks to make a tanda."));
            return {};
        }
    }
    for (const TandaSpan& span : std::as_const(m_spans)) {
        if (rangesOverlap(positions.first() - 1,
                    positions.size(),
                    span.anchorPosition - 1,
                    span.members.size())) {
            setError(pError, tr("The selection overlaps an existing tanda."));
            return {};
        }
    }

    TandaSpan span;
    span.id = QUuid::createUuid();
    span.type = type;
    span.anchorPosition = positions.first();
    span.members.reserve(positions.size());
    for (int position : std::as_const(positions)) {
        span.members.append(m_queueSnapshot.at(position - 1));
    }
    const QUuid id = span.id;
    m_spans.append(std::move(span));
    sortSpans();
    save();
    emit spansChanged();
    return id;
}

int TandaQueueState::indexById(const QUuid& id) const {
    for (int index = 0; index < m_spans.size(); ++index) {
        if (m_spans.at(index).id == id) {
            return index;
        }
    }
    return -1;
}

const TandaSpan* TandaQueueState::spanById(const QUuid& id) const {
    const int index = indexById(id);
    return index >= 0 ? &m_spans.at(index) : nullptr;
}

const TandaSpan* TandaQueueState::spanAtPosition(int oneBasedPosition) const {
    for (const TandaSpan& span : m_spans) {
        if (span.anchorPosition <= oneBasedPosition &&
                oneBasedPosition < span.anchorPosition + span.members.size()) {
            return &span;
        }
    }
    return nullptr;
}

bool TandaQueueState::ungroup(const QUuid& id) {
    const int index = indexById(id);
    if (index < 0) {
        return false;
    }
    m_spans.removeAt(index);
    save();
    emit spansChanged();
    return true;
}

bool TandaQueueState::changeType(const QUuid& id, TandaType type) {
    const int index = indexById(id);
    if (index < 0 || m_spans.at(index).type == type) {
        return false;
    }
    m_spans[index].type = type;
    save();
    emit spansChanged();
    return true;
}

bool TandaQueueState::setCollapsed(const QUuid& id, bool collapsed) {
    const int index = indexById(id);
    if (index < 0 || m_spans.at(index).collapsed == collapsed) {
        return false;
    }
    m_spans[index].collapsed = collapsed;
    save();
    emit spansChanged();
    return true;
}

int TandaQueueState::dissolveIf(
        const std::function<bool(const TandaSpan&)>& predicate) {
    const int oldSize = m_spans.size();
    m_spans.erase(std::remove_if(m_spans.begin(), m_spans.end(), predicate),
            m_spans.end());
    const int removed = oldSize - m_spans.size();
    if (removed > 0) {
        save();
        emit spansChanged();
    }
    return removed;
}

int TandaQueueState::dissolveForRemoval(
        const QVector<int>& oneBasedPositions) {
    QVector<int> positions = oneBasedPositions;
    positions.erase(std::remove_if(positions.begin(),
                            positions.end(),
                            [this](int position) {
                                return position <= 0 ||
                                        position > m_queueSnapshot.size();
                            }),
            positions.end());
    std::sort(positions.begin(), positions.end());
    positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
    if (positions.isEmpty()) {
        return 0;
    }

    const int oldSize = m_spans.size();
    m_spans.erase(std::remove_if(m_spans.begin(),
                          m_spans.end(),
                          [&positions](const TandaSpan& span) {
                              for (int position : positions) {
                                  if (span.anchorPosition <= position &&
                                          position < span.anchorPosition +
                                                          span.members.size()) {
                                      return true;
                                  }
                              }
                              return false;
                          }),
            m_spans.end());
    for (TandaSpan& span : m_spans) {
        span.anchorPosition -= static_cast<int>(std::count_if(positions.cbegin(),
                positions.cend(),
                [&span](int position) {
                    return position < span.anchorPosition;
                }));
    }
    for (auto it = positions.crbegin(); it != positions.crend(); ++it) {
        m_queueSnapshot.removeAt(*it - 1);
    }

    const int removed = oldSize - m_spans.size();
    save();
    emit spansChanged();
    return removed;
}

int TandaQueueState::dissolveForInsertion(int oneBasedPosition) {
    return dissolveIf([oneBasedPosition](const TandaSpan& span) {
        return span.anchorPosition < oneBasedPosition &&
                oneBasedPosition < span.anchorPosition + span.members.size();
    });
}

int TandaQueueState::dissolveForIndividualMove(
        int oldPosition, int newPosition) {
    if (oldPosition <= 0 || oldPosition > m_queueSnapshot.size() ||
            newPosition <= 0 || newPosition > m_queueSnapshot.size() ||
            oldPosition == newPosition) {
        return 0;
    }

    const auto mappedPosition = [oldPosition, newPosition](int position) {
        if (position == oldPosition) {
            return newPosition;
        }
        if (oldPosition < newPosition && oldPosition < position &&
                position <= newPosition) {
            return position - 1;
        }
        if (newPosition < oldPosition && newPosition <= position &&
                position < oldPosition) {
            return position + 1;
        }
        return position;
    };

    const int oldSize = m_spans.size();
    QVector<TandaSpan> surviving;
    for (TandaSpan span : std::as_const(m_spans)) {
        const int start = span.anchorPosition;
        const int end = start + span.members.size() - 1;
        if (start <= oldPosition && oldPosition <= end) {
            continue;
        }
        const int mappedStart = mappedPosition(start);
        bool stillConsecutive = true;
        for (int position = start + 1; position <= end; ++position) {
            if (mappedPosition(position) != mappedStart + position - start) {
                stillConsecutive = false;
                break;
            }
        }
        if (!stillConsecutive) {
            continue;
        }
        span.anchorPosition = mappedStart;
        surviving.append(std::move(span));
    }
    m_spans = std::move(surviving);

    const TrackId movedTrack = m_queueSnapshot.at(oldPosition - 1);
    m_queueSnapshot.removeAt(oldPosition - 1);
    m_queueSnapshot.insert(newPosition - 1, movedTrack);
    sortSpans();
    save();
    emit spansChanged();
    return oldSize - m_spans.size();
}

int TandaQueueState::dissolveForReorder(
        const QVector<int>& oneBasedPositions) {
    const int removed = dissolveIf([&oneBasedPositions](const TandaSpan& span) {
        for (int position : oneBasedPositions) {
            if (span.anchorPosition <= position &&
                    position < span.anchorPosition + span.members.size()) {
                return true;
            }
        }
        return false;
    });
    // PlaylistDAO emits tracksMoved synchronously after the shuffle. Spans not
    // selected by the shuffle remain at the same positions and may survive;
    // never infer a moved tanda at a different anchor from matching IDs.
    m_preserveUnchangedSpansOnNextReconcile = true;
    return removed;
}

bool TandaQueueState::applyWholeTandaMove(
        const QUuid& id, int newAnchorPosition) {
    const int index = indexById(id);
    if (index < 0) {
        return false;
    }
    const int oldStart = m_spans.at(index).anchorPosition - 1;
    const int length = m_spans.at(index).members.size();
    const int newStart = newAnchorPosition - 1;
    if (newStart < 0 || newStart + length > m_queueSnapshot.size()) {
        return false;
    }
    if (newStart == oldStart) {
        return true;
    }

    const QVector<TrackId> moved = m_queueSnapshot.mid(oldStart, length);
    m_queueSnapshot.remove(oldStart, length);
    for (int offset = 0; offset < moved.size(); ++offset) {
        m_queueSnapshot.insert(newStart + offset, moved.at(offset));
    }

    for (TandaSpan& span : m_spans) {
        const int start = span.anchorPosition - 1;
        if (span.id == id) {
            span.anchorPosition = newAnchorPosition;
        } else if (newStart < oldStart && newStart <= start && start < oldStart) {
            span.anchorPosition += length;
        } else if (oldStart < newStart && oldStart + length <= start &&
                start < newStart + length) {
            span.anchorPosition -= length;
        }
    }
    sortSpans();
    save();
    emit spansChanged();
    return true;
}

void TandaQueueState::sortSpans() {
    std::sort(m_spans.begin(), m_spans.end(), [](const TandaSpan& left, const TandaSpan& right) {
        return left.anchorPosition < right.anchorPosition;
    });
}

void TandaQueueState::save() {
    if (!m_pConfig) {
        return;
    }
    QJsonArray serializedSpans;
    for (const TandaSpan& span : std::as_const(m_spans)) {
        QJsonArray members;
        for (TrackId member : span.members) {
            members.append(member.toVariant().toInt());
        }
        QJsonObject object;
        object.insert(QStringLiteral("id"), span.id.toString(QUuid::WithoutBraces));
        object.insert(QStringLiteral("type"), typeToString(span.type));
        object.insert(QStringLiteral("name"), span.name);
        object.insert(QStringLiteral("members"), members);
        object.insert(QStringLiteral("anchor"), span.anchorPosition);
        object.insert(QStringLiteral("collapsed"), span.collapsed);
        serializedSpans.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), kSerializationVersion);
    root.insert(QStringLiteral("spans"), serializedSpans);
    m_pConfig->setValue(kTandaStateConfigKey,
            QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}
