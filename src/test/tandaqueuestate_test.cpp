#include "library/autodj/tandaqueuestate.h"
#include "library/autodj/tandaqueuemodel.h"

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

#include "library/dao/playlistdao.h"
#include "library/dao/trackschema.h"
#include "library/playlisttablemodel.h"
#include "library/trackcollection.h"
#include "test/librarytest.h"
#include "test/mixxxtest.h"

namespace {

TrackId id(int value) {
    return TrackId(QVariant(value));
}

QVector<TrackId> queue(std::initializer_list<int> values) {
    QVector<TrackId> result;
    for (int value : values) {
        result.append(id(value));
    }
    return result;
}

const ConfigKey kTandaStateConfigKey(
        QStringLiteral("[TangoMode]"), QStringLiteral("AutoDjTandasV1"));

} // namespace

class TandaQueueStateTest : public MixxxTest {
};

TEST_F(TandaQueueStateTest, ClassificationRequiresConsecutiveOrdinaryRows) {
    TandaQueueState state(config());
    state.restore(queue({1, 2, 3, 4, 5}));

    QString error;
    EXPECT_TRUE(state.classify({}, TandaType::Tango, &error).isNull());
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(state.classify({1, 3}, TandaType::Tango).isNull());

    const QUuid tanda = state.classify({3, 2}, TandaType::Vals);
    ASSERT_FALSE(tanda.isNull());
    ASSERT_EQ(1, state.spans().size());
    EXPECT_EQ(2, state.spans().first().anchorPosition);
    EXPECT_EQ(TandaType::Vals, state.spans().first().type);
    EXPECT_EQ(queue({2, 3}), state.spans().first().members);

    EXPECT_TRUE(state.classify({3, 4}, TandaType::Milonga).isNull());
    EXPECT_EQ(1, state.spans().size());
}

TEST_F(TandaQueueStateTest, RestoreUsesExactMembersAndRefusesDuplicateAmbiguity) {
    TandaQueueState original(config());
    original.restore(queue({1, 2, 3, 4}));
    const QUuid id = original.classify({2, 3}, TandaType::Milonga);
    ASSERT_FALSE(id.isNull());
    ASSERT_TRUE(original.setCollapsed(id, true));
    saveAndReloadConfig();

    // The unique sequence moved because something was inserted before it.
    TandaQueueState restored(config());
    restored.restore(queue({9, 1, 2, 3, 4}));
    ASSERT_EQ(1, restored.spans().size());
    EXPECT_EQ(3, restored.spans().first().anchorPosition);
    EXPECT_TRUE(restored.spans().first().collapsed);
    EXPECT_EQ(TandaType::Milonga, restored.spans().first().type);

    QJsonObject ambiguousSpan;
    ambiguousSpan.insert(QStringLiteral("id"),
            QUuid::createUuid().toString(QUuid::WithoutBraces));
    ambiguousSpan.insert(QStringLiteral("type"), QStringLiteral("tango"));
    ambiguousSpan.insert(QStringLiteral("name"), QString());
    ambiguousSpan.insert(QStringLiteral("members"), QJsonArray({1, 2}));
    // Anchor 2 matches neither occurrence; resolving by track IDs would have
    // two equally valid answers and must be refused.
    ambiguousSpan.insert(QStringLiteral("anchor"), 2);
    ambiguousSpan.insert(QStringLiteral("collapsed"), false);
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("spans"), QJsonArray({ambiguousSpan}));
    config()->setValue(kTandaStateConfigKey,
            QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));

    TandaQueueState ambiguous(config());
    ambiguous.restore(queue({1, 2, 1, 2}));
    EXPECT_TRUE(ambiguous.spans().isEmpty());
}

TEST_F(TandaQueueStateTest, MembershipMutationsDissolveAndAppendDoesNotReclassify) {
    const QVector<TrackId> initial = queue({1, 2, 3, 4});

    TandaQueueState removed{UserSettingsPointer()};
    removed.restore(initial);
    ASSERT_FALSE(removed.classify({2, 3}, TandaType::Tango).isNull());
    EXPECT_EQ(1, removed.dissolveForRemoval({2}));
    removed.reconcileQueue(queue({1, 3, 4}));
    removed.reconcileQueue(queue({1, 3, 4, 2}));
    EXPECT_TRUE(removed.spans().isEmpty());

    TandaQueueState inserted{UserSettingsPointer()};
    inserted.restore(initial);
    ASSERT_FALSE(inserted.classify({2, 3}, TandaType::Tango).isNull());
    EXPECT_EQ(1, inserted.dissolveForInsertion(3));

    TandaQueueState replaced{UserSettingsPointer()};
    replaced.restore(initial);
    ASSERT_FALSE(replaced.classify({2, 3}, TandaType::Tango).isNull());
    EXPECT_EQ(1, replaced.dissolveForRemoval({3}));
    replaced.reconcileQueue(queue({1, 2, 9, 4}));
    EXPECT_TRUE(replaced.spans().isEmpty());

    TandaQueueState moved{UserSettingsPointer()};
    moved.restore(initial);
    ASSERT_FALSE(moved.classify({2, 3}, TandaType::Tango).isNull());
    EXPECT_EQ(1, moved.dissolveForIndividualMove(2, 4));

    TandaQueueState appended{UserSettingsPointer()};
    appended.restore(initial);
    const QUuid appendedId = appended.classify({2, 3}, TandaType::Tango);
    appended.reconcileQueue(queue({1, 2, 3, 4, 5}));
    ASSERT_NE(nullptr, appended.spanById(appendedId));
    EXPECT_EQ(1, appended.spans().size());
}

TEST_F(TandaQueueStateTest, WholeBlockMovePreservesMetadataAroundDuplicateTracks) {
    TandaQueueState state(config());
    state.restore(queue({1, 2, 2, 3, 4}));
    const QUuid duplicateTanda = state.classify({2, 3}, TandaType::NuevoAlternative);
    const QUuid otherTanda = state.classify({4}, TandaType::Vals);
    ASSERT_FALSE(duplicateTanda.isNull());
    ASSERT_FALSE(otherTanda.isNull());
    ASSERT_TRUE(state.setCollapsed(duplicateTanda, true));

    ASSERT_TRUE(state.applyWholeTandaMove(duplicateTanda, 4));
    EXPECT_EQ(queue({1, 3, 4, 2, 2}), state.queueSnapshot());
    const TandaSpan* pMoved = state.spanById(duplicateTanda);
    ASSERT_NE(nullptr, pMoved);
    EXPECT_EQ(4, pMoved->anchorPosition);
    EXPECT_EQ(TandaType::NuevoAlternative, pMoved->type);
    EXPECT_TRUE(pMoved->collapsed);
    ASSERT_NE(nullptr, state.spanById(otherTanda));
    EXPECT_EQ(2, state.spanById(otherTanda)->anchorPosition);
}

TEST_F(TandaQueueStateTest, KnownEditsPreserveUnaffectedTandasAndShiftAnchors) {
    TandaQueueState removal{UserSettingsPointer()};
    removal.restore(queue({1, 2, 3, 4, 5, 6}));
    const QUuid removalTanda =
            removal.classify({3, 4}, TandaType::Tango);
    ASSERT_FALSE(removalTanda.isNull());
    EXPECT_EQ(0, removal.dissolveForRemoval({1, 6}));
    ASSERT_NE(nullptr, removal.spanById(removalTanda));
    EXPECT_EQ(2, removal.spanById(removalTanda)->anchorPosition);
    EXPECT_EQ(queue({2, 3, 4, 5}), removal.queueSnapshot());

    TandaQueueState moveAround{UserSettingsPointer()};
    moveAround.restore(queue({1, 2, 3, 4, 5, 6}));
    const QUuid movedAroundTanda =
            moveAround.classify({3, 4}, TandaType::Vals);
    ASSERT_FALSE(movedAroundTanda.isNull());
    EXPECT_EQ(0, moveAround.dissolveForIndividualMove(1, 6));
    ASSERT_NE(nullptr, moveAround.spanById(movedAroundTanda));
    EXPECT_EQ(2, moveAround.spanById(movedAroundTanda)->anchorPosition);
    EXPECT_EQ(queue({2, 3, 4, 5, 6, 1}), moveAround.queueSnapshot());

    // Inserting a moved leaf between two members dissolves the tanda, even
    // when the destination was the tanda's old first position.
    TandaQueueState split{UserSettingsPointer()};
    split.restore(queue({1, 2, 3, 4, 5, 6}));
    ASSERT_FALSE(split.classify({3, 4}, TandaType::Milonga).isNull());
    EXPECT_EQ(1, split.dissolveForIndividualMove(1, 3));
    EXPECT_TRUE(split.spans().isEmpty());

    TandaQueueState shuffled{UserSettingsPointer()};
    shuffled.restore(queue({1, 2, 3, 4, 5, 6}));
    const QUuid unaffected = shuffled.classify({3, 4}, TandaType::Tango);
    ASSERT_FALSE(unaffected.isNull());
    EXPECT_EQ(0, shuffled.dissolveForReorder({1, 2, 5, 6}));
    shuffled.reconcileQueue(queue({2, 1, 3, 4, 6, 5}));
    EXPECT_NE(nullptr, shuffled.spanById(unaffected));
}

class TandaQueueDaoTest : public LibraryTest {
  protected:
    TrackId addTrack(const QString& name) {
        TrackPointer pTrack = getOrAddTrackByLocation(
                getTestDir().filePath(QStringLiteral("id3-test-data/") + name));
        EXPECT_TRUE(pTrack);
        return pTrack ? pTrack->getId() : TrackId();
    }
};

TEST_F(TandaQueueDaoTest, AtomicRangeMoveHandlesBoundsAndDuplicateOccurrences) {
    PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
    const int playlistId =
            dao.createPlaylist(QStringLiteral("Tanda range move test"),
                    PlaylistDAO::PLHT_NOT_HIDDEN);
    ASSERT_GE(playlistId, 0);
    const TrackId a = addTrack(QStringLiteral("artist.mp3"));
    const TrackId b = addTrack(QStringLiteral("cover-test-jpg.mp3"));
    const TrackId c = addTrack(QStringLiteral("cover-test-png.mp3"));
    const TrackId d = addTrack(QStringLiteral("cover-test-vbr.mp3"));
    ASSERT_TRUE(a.isValid());
    ASSERT_TRUE(b.isValid());
    ASSERT_TRUE(c.isValid());
    ASSERT_TRUE(d.isValid());
    ASSERT_TRUE(dao.appendTracksToPlaylist({a, b, b, c, d}, playlistId));

    QSignalSpy movedSpy(&dao, &PlaylistDAO::tracksMoved);
    ASSERT_TRUE(dao.moveTrackRange(playlistId, 2, 3, 4));
    EXPECT_EQ(QVector<TrackId>({a, c, d, b, b}),
            dao.getTrackIdsInPlaylistOrder(playlistId));
    EXPECT_EQ(1, movedSpy.count());

    ASSERT_TRUE(dao.moveTrackRange(playlistId, 4, 5, 1));
    EXPECT_EQ(QVector<TrackId>({b, b, a, c, d}),
            dao.getTrackIdsInPlaylistOrder(playlistId));
    EXPECT_EQ(2, movedSpy.count());

    EXPECT_FALSE(dao.moveTrackRange(playlistId, 0, 1, 2));
    EXPECT_FALSE(dao.moveTrackRange(playlistId, 1, 2, 5));
    EXPECT_EQ(2, movedSpy.count());
}

TEST_F(TandaQueueDaoTest, OutlineMapsHeadersLeavesAndSharedCollapseState) {
    PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
    const int playlistId =
            dao.createPlaylist(QStringLiteral("Tanda outline test"),
                    PlaylistDAO::PLHT_NOT_HIDDEN);
    ASSERT_GE(playlistId, 0);
    const TrackId a = addTrack(QStringLiteral("artist.mp3"));
    const TrackId b = addTrack(QStringLiteral("cover-test-jpg.mp3"));
    const TrackId c = addTrack(QStringLiteral("cover-test-png.mp3"));
    const TrackId d = addTrack(QStringLiteral("cover-test-vbr.mp3"));
    ASSERT_TRUE(dao.appendTracksToPlaylist({a, b, c, d}, playlistId));

    PlaylistTableModel source(
            nullptr, trackCollectionManager(), "tanda_outline_test");
    source.selectPlaylist(playlistId);
    source.select();
    ASSERT_EQ(4, source.rowCount());

    TandaQueueState state{UserSettingsPointer()};
    state.restore({a, b, c, d});
    const QUuid tanda = state.classify({2, 3}, TandaType::Vals);
    ASSERT_FALSE(tanda.isNull());

    TandaQueueModel first(&source, &state);
    TandaQueueModel second(&source, &state);
    ASSERT_EQ(5, first.rowCount());
    EXPECT_FALSE(first.isHeaderRow(0));
    EXPECT_TRUE(first.isHeaderRow(1));
    EXPECT_EQ(tanda, first.tandaIdForRow(1));
    EXPECT_FALSE(first.mapToSource(first.index(1, 0)).isValid());
    EXPECT_EQ(1, first.mapToSource(first.index(2, 0)).row());
    EXPECT_EQ(3, first.mapToSource(first.index(4, 0)).row());
    EXPECT_FALSE(first.getTrack(first.index(1, 0)));
    EXPECT_EQ(b, first.getTrackId(first.index(2, 0)));
    EXPECT_FALSE(first.flags(first.index(1, 0)).testFlag(Qt::ItemIsDragEnabled));
    EXPECT_EQ(QList<int>({2, 3}),
            first.getSelectedPositions(
                    {first.index(2, 0), first.index(3, 0)}));
    const int summaryColumn = first.summaryColumn();
    EXPECT_FALSE(first.data(first.index(0, summaryColumn))
                         .toString()
                         .startsWith(QStringLiteral("    ")));
    EXPECT_TRUE(first.data(first.index(2, summaryColumn))
                        .toString()
                        .startsWith(QStringLiteral("    ")));

    bool foundSummary = false;
    for (int column = 0; column < first.columnCount(); ++column) {
        foundSummary = foundSummary ||
                first.data(first.index(1, column))
                        .toString()
                        .contains(QStringLiteral("Vals"));
    }
    EXPECT_TRUE(foundSummary);

    QSignalSpy firstReset(&first, &QAbstractItemModel::modelReset);
    QSignalSpy secondReset(&second, &QAbstractItemModel::modelReset);
    ASSERT_TRUE(state.setCollapsed(tanda, true));
    EXPECT_EQ(3, first.rowCount());
    EXPECT_EQ(3, second.rowCount());
    EXPECT_EQ(1, firstReset.count());
    EXPECT_EQ(1, secondReset.count());
    EXPECT_TRUE(first.isHeaderRow(1));
    EXPECT_EQ(3, first.mapToSource(first.index(2, 0)).row());
    EXPECT_FALSE(first.mapFromSource(source.index(1, 0)).isValid());

    ASSERT_TRUE(state.setCollapsed(tanda, false));
    EXPECT_EQ(5, first.rowCount());
    EXPECT_EQ(1, first.mapToSource(first.index(2, 0)).row());
}
