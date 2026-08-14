#pragma once

#include <QObject>
#include <QString>
#include <QUuid>
#include <QVector>
#include <functional>

#include "preferences/usersettings.h"
#include "track/trackid.h"

enum class TandaType {
    Tango,
    Vals,
    Milonga,
    NuevoAlternative,
};

struct TandaSpan {
    QUuid id;
    TandaType type{TandaType::Tango};
    QString name;
    QVector<TrackId> members;
    // One-based position in the underlying Auto DJ playlist.
    int anchorPosition{0};
    bool collapsed{false};
};

/// Optional grouping metadata for the otherwise-flat Auto DJ queue.
///
/// Track IDs are intentionally stored as an ordered occurrence sequence. The
/// anchor disambiguates repeated tracks while the queue is unchanged; restore
/// refuses to guess when the same sequence occurs more than once.
class TandaQueueState : public QObject {
    Q_OBJECT

  public:
    explicit TandaQueueState(UserSettingsPointer pConfig, QObject* pParent = nullptr);

    const QVector<TandaSpan>& spans() const {
        return m_spans;
    }
    const QVector<TrackId>& queueSnapshot() const {
        return m_queueSnapshot;
    }

    /// Loads persisted spans and validates them against queue.
    void restore(const QVector<TrackId>& queue);
    /// Reconciles an ordinary (not an explicit whole-tanda move) queue edit.
    void reconcileQueue(const QVector<TrackId>& queue);

    QUuid classify(const QVector<int>& oneBasedPositions,
            TandaType type,
            QString* pError = nullptr);
    bool ungroup(const QUuid& id);
    bool changeType(const QUuid& id, TandaType type);
    bool setCollapsed(const QUuid& id, bool collapsed);

    const TandaSpan* spanById(const QUuid& id) const;
    const TandaSpan* spanAtPosition(int oneBasedPosition) const;

    /// Dissolution hooks used before ordinary queue operations.
    int dissolveForRemoval(const QVector<int>& oneBasedPositions);
    int dissolveForInsertion(int oneBasedPosition);
    int dissolveForIndividualMove(int oldPosition, int newPosition);
    int dissolveForReorder(const QVector<int>& oneBasedPositions);

    /// Updates the snapshot and span anchors after the DAO atomically moved the
    /// complete block. This is the sole move path that preserves a tanda.
    bool applyWholeTandaMove(const QUuid& id, int newAnchorPosition);

    static QString typeToString(TandaType type);

  signals:
    void spansChanged();

  private:
    static bool sequenceMatches(const QVector<TrackId>& queue,
            int zeroBasedStart,
            const QVector<TrackId>& members);
    static QVector<int> matchingStarts(
            const QVector<TrackId>& queue, const QVector<TrackId>& members);
    static bool rangesOverlap(int firstStart,
            int firstLength,
            int secondStart,
            int secondLength);
    static bool typeFromString(const QString& value, TandaType* pType);

    void save();
    void sortSpans();
    int indexById(const QUuid& id) const;
    int dissolveIf(const std::function<bool(const TandaSpan&)>& predicate);

    UserSettingsPointer m_pConfig;
    QVector<TandaSpan> m_spans;
    QVector<TrackId> m_queueSnapshot;
    bool m_preserveUnchangedSpansOnNextReconcile{false};
};
