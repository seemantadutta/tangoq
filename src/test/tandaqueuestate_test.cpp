// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/tandaqueuestate.h"

#include <gtest/gtest.h>

#include <QBrush>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

#include "control/controlobject.h"
#include "control/controlpotmeter.h"
#include "library/autodj/cortinaregistry.h"
#include "library/autodj/performanceregistry.h"
#include "library/autodj/tandacolorpalette.h"
#include "library/autodj/tandaqueuemodel.h"
#include "library/dao/playlistdao.h"
#include "library/dao/trackschema.h"
#include "library/playlisttablemodel.h"
#include "library/trackcollection.h"
#include "mixer/playerinfo.h"
#include "test/librarytest.h"
#include "test/mixxxtest.h"
#include "track/track.h"

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

// Groups whose ControlObjects PlayerInfo polls; see TandaQueueDaoTest.
const QString kMasterGroup = QStringLiteral("[Master]");
const QString kAppGroup = QStringLiteral("[App]");

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
    EXPECT_TRUE(state.spans().first().collapsed);
    EXPECT_EQ(2, state.spans().first().anchorPosition);
    EXPECT_EQ(TandaType::Vals, state.spans().first().type);
    EXPECT_EQ(queue({2, 3}), state.spans().first().members);

    EXPECT_TRUE(state.classify({3, 4}, TandaType::Milonga).isNull());
    EXPECT_EQ(1, state.spans().size());
}

TEST_F(TandaQueueStateTest, SetNameStoresPersistsAndRevertsToAuto) {
    TandaQueueState state(config());
    state.restore(queue({1, 2, 3, 4}));
    const QUuid tanda = state.classify({1, 2, 3}, TandaType::Vals);
    ASSERT_FALSE(tanda.isNull());
    // A fresh tanda has no custom name (display falls back to the type label).
    EXPECT_TRUE(state.spans().first().name.isEmpty());

    // Setting a name stores it and reports the change; a no-op returns false.
    EXPECT_TRUE(state.setName(tanda, QStringLiteral("La Cumparsita")));
    EXPECT_EQ(QStringLiteral("La Cumparsita"), state.spans().first().name);
    EXPECT_FALSE(state.setName(tanda, QStringLiteral("La Cumparsita")));

    // The custom name survives a save/restore round-trip.
    TandaQueueState restored(config());
    restored.restore(queue({1, 2, 3, 4}));
    ASSERT_EQ(1, restored.spans().size());
    EXPECT_EQ(QStringLiteral("La Cumparsita"), restored.spans().first().name);

    // An empty name reverts to the automatic label (an empty stored name).
    const QUuid restoredId = restored.spans().first().id;
    EXPECT_TRUE(restored.setName(restoredId, QString()));
    EXPECT_TRUE(restored.spans().first().name.isEmpty());
}

TEST_F(TandaQueueStateTest, RestoreUsesExactMembersAndRefusesDuplicateAmbiguity) {
    TandaQueueState original(config());
    original.restore(queue({1, 2, 3, 4}));
    const QUuid id = original.classify({2, 3}, TandaType::Milonga);
    ASSERT_FALSE(id.isNull());
    ASSERT_TRUE(original.spans().first().collapsed);
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
    ASSERT_TRUE(state.spanById(duplicateTanda)->collapsed);

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
    // These tests build a PlaylistTableModel, whose BaseTrackTableModel base
    // connects to PlayerInfo::instance() in its constructor. Create the singleton
    // for the fixture's lifetime (as AutoDJProcessorTest / PlayerManagerTest do);
    // otherwise a Debug build trips the "s_pPlayerInfo" DEBUG_ASSERT, which raises
    // SIGINT and shows up as a ctest INTERRUPT in the coverage job.
    //
    // PlayerInfo also starts a timer whose updateCurrentPlayingDeck() polls
    // [Master],crossfader and [App],num_decks. Those ControlObjects must exist or
    // the same Debug build trips the getControl NoAssertIfMissing DEBUG_ASSERT
    // (again a SIGINT / ctest INTERRUPT). num_decks = 0 keeps PlayerInfo from
    // touching any per-deck controls. The [App] counts mirror what a real
    // PlayerManager publishes, matching AutoDJProcessorTest's fixture.
    TandaQueueDaoTest()
            : m_crossfader(ConfigKey(kMasterGroup, QStringLiteral("crossfader")),
                      -1.0,
                      1.0),
              m_numDecks(ConfigKey(kAppGroup, QStringLiteral("num_decks"))),
              m_numSamplers(ConfigKey(kAppGroup, QStringLiteral("num_samplers"))),
              m_numPreviewDecks(
                      ConfigKey(kAppGroup, QStringLiteral("num_preview_decks"))) {
        m_numDecks.set(0.0);
        PlayerInfo::create();
    }
    ~TandaQueueDaoTest() override {
        PlayerInfo::destroy();
    }

    TrackId addTrack(const QString& name) {
        TrackPointer pTrack = getOrAddTrackByLocation(
                getTestDir().filePath(QStringLiteral("id3-test-data/") + name));
        EXPECT_TRUE(pTrack);
        return pTrack ? pTrack->getId() : TrackId();
    }

  private:
    ControlPotmeter m_crossfader;
    ControlObject m_numDecks;
    ControlObject m_numSamplers;
    ControlObject m_numPreviewDecks;
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
    ASSERT_TRUE(state.setCollapsed(tanda, false));

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

TEST_F(TandaQueueDaoTest, TandaTypeColumnShowsLetterOnHeaderOnly) {
    PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
    const int playlistId =
            dao.createPlaylist(QStringLiteral("Tanda type column test"),
                    PlaylistDAO::PLHT_NOT_HIDDEN);
    ASSERT_GE(playlistId, 0);
    const TrackId a = addTrack(QStringLiteral("artist.mp3"));
    const TrackId b = addTrack(QStringLiteral("cover-test-jpg.mp3"));
    const TrackId c = addTrack(QStringLiteral("cover-test-png.mp3"));
    const TrackId d = addTrack(QStringLiteral("cover-test-vbr.mp3"));
    ASSERT_TRUE(dao.appendTracksToPlaylist({a, b, c, d}, playlistId));

    PlaylistTableModel source(
            nullptr, trackCollectionManager(), "tanda_type_column_test");
    source.selectPlaylist(playlistId);
    source.select();
    ASSERT_EQ(4, source.rowCount());

    TandaQueueState state{UserSettingsPointer()};
    state.restore({a, b, c, d});
    const QUuid tanda = state.classify({2, 3}, TandaType::Vals);
    ASSERT_FALSE(tanda.isNull());
    ASSERT_TRUE(state.setCollapsed(tanda, false));

    TandaQueueModel model(&source, &state);
    ASSERT_EQ(5, model.rowCount());

    // The appended column sits one past the source columns.
    const int typeCol = model.columnCount() - 1;
    EXPECT_EQ(source.columnCount() + 1, model.columnCount());
    EXPECT_EQ(QStringLiteral("Item Type"),
            model.headerData(typeCol, Qt::Horizontal).toString());

    // The tanda header carries its letter; loose tracks and constituent tracks
    // stay blank in that column.
    EXPECT_TRUE(model.isHeaderRow(1));
    EXPECT_EQ(QStringLiteral("V"),
            model.data(model.index(1, typeCol)).toString());
    EXPECT_TRUE(model.data(model.index(0, typeCol)).toString().isEmpty());
    EXPECT_TRUE(model.data(model.index(2, typeCol)).toString().isEmpty());

    // A real, user-selectable, non-sortable column that never maps to a source
    // cell.
    EXPECT_FALSE(model.isColumnInternal(typeCol));
    EXPECT_FALSE(model.isColumnSortable(typeCol));
    EXPECT_FALSE(model.mapToSource(model.index(1, typeCol)).isValid());
    EXPECT_FALSE(model.mapToSource(model.index(2, typeCol)).isValid());
}

TEST_F(TandaQueueDaoTest, TandaTypeColumnMarksCortinaTracks) {
    PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
    const int playlistId =
            dao.createPlaylist(QStringLiteral("Tanda cortina column test"),
                    PlaylistDAO::PLHT_NOT_HIDDEN);
    ASSERT_GE(playlistId, 0);
    const TrackId a = addTrack(QStringLiteral("artist.mp3"));
    const TrackId b = addTrack(QStringLiteral("cover-test-jpg.mp3"));
    const TrackId c = addTrack(QStringLiteral("cover-test-png.mp3"));
    ASSERT_TRUE(dao.appendTracksToPlaylist({a, b, c}, playlistId));

    PlaylistTableModel source(
            nullptr, trackCollectionManager(), "tanda_cortina_column_test");
    source.selectPlaylist(playlistId);
    source.select();
    source.setShowCortinaMarks(true);
    ASSERT_EQ(3, source.rowCount());

    TandaQueueState state{UserSettingsPointer()};
    state.restore({a, b, c});

    TandaQueueModel model(&source, &state);
    ASSERT_EQ(3, model.rowCount());
    const int typeCol = model.columnCount() - 1;

    // Row 1 (track b) is an ordinary loose track until tagged as a cortina.
    EXPECT_TRUE(model.data(model.index(1, typeCol)).toString().isEmpty());
    CortinaRegistry::instance().mark(b);
    EXPECT_EQ(QStringLiteral("c"),
            model.data(model.index(1, typeCol)).toString());
    EXPECT_TRUE(model.data(model.index(0, typeCol)).toString().isEmpty());
    CortinaRegistry::instance().unmark(b);
    EXPECT_TRUE(model.data(model.index(1, typeCol)).toString().isEmpty());
}

TEST_F(TandaQueueDaoTest, TandaTypeColumnMarksPerformanceTracks) {
    PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
    const int playlistId =
            dao.createPlaylist(QStringLiteral("Tanda performance column test"),
                    PlaylistDAO::PLHT_NOT_HIDDEN);
    ASSERT_GE(playlistId, 0);
    const TrackId a = addTrack(QStringLiteral("artist.mp3"));
    const TrackId b = addTrack(QStringLiteral("cover-test-jpg.mp3"));
    ASSERT_TRUE(dao.appendTracksToPlaylist({a, b}, playlistId));

    PlaylistTableModel source(
            nullptr, trackCollectionManager(), "tanda_performance_column_test");
    source.selectPlaylist(playlistId);
    source.select();
    source.setShowCortinaMarks(true);
    ASSERT_EQ(2, source.rowCount());

    TandaQueueState state{UserSettingsPointer()};
    state.restore({a, b});

    TandaQueueModel model(&source, &state);
    ASSERT_EQ(2, model.rowCount());
    const int typeCol = model.columnCount() - 1;

    EXPECT_TRUE(model.data(model.index(0, typeCol)).toString().isEmpty());
    PerformanceRegistry::instance().mark(a);
    EXPECT_EQ(QStringLiteral("p"),
            model.data(model.index(0, typeCol)).toString());

    // Cortina takes precedence in the column when both marks somehow coexist
    // (the menu keeps them mutually exclusive; this pins the display rule).
    CortinaRegistry::instance().mark(a);
    EXPECT_EQ(QStringLiteral("c"),
            model.data(model.index(0, typeCol)).toString());

    CortinaRegistry::instance().unmark(a);
    PerformanceRegistry::instance().unmark(a);
    EXPECT_TRUE(model.data(model.index(0, typeCol)).toString().isEmpty());
}

TEST_F(TandaQueueDaoTest, RowColorsFollowCategoryWithCortinaPrecedence) {
    PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
    const int playlistId =
            dao.createPlaylist(QStringLiteral("Tanda row color test"),
                    PlaylistDAO::PLHT_NOT_HIDDEN);
    ASSERT_GE(playlistId, 0);
    const TrackId regular = addTrack(QStringLiteral("artist.mp3"));
    const TrackId valsA = addTrack(QStringLiteral("cover-test-jpg.mp3"));
    const TrackId valsB = addTrack(QStringLiteral("cover-test-png.mp3"));
    const TrackId special = addTrack(QStringLiteral("cover-test-vbr.mp3"));
    ASSERT_TRUE(dao.appendTracksToPlaylist(
            {regular, valsA, valsB, special}, playlistId));

    PlaylistTableModel source(
            nullptr, trackCollectionManager(), "tanda_row_color_test");
    source.selectPlaylist(playlistId);
    source.select();
    ASSERT_EQ(4, source.rowCount());

    TandaQueueState state{UserSettingsPointer()};
    state.restore({regular, valsA, valsB, special});
    const QUuid tanda = state.classify({2, 3}, TandaType::Vals);
    ASSERT_FALSE(tanda.isNull());
    ASSERT_TRUE(state.setCollapsed(tanda, false));

    TandaColorPalette palette(config());
    TandaQueueModel model(&source, &state, nullptr, nullptr, &palette);
    ASSERT_EQ(5, model.rowCount());

    const auto backgroundForRow = [&model](int row) {
        return model.data(model.index(row, 0), Qt::BackgroundRole)
                .value<QBrush>()
                .color();
    };
    const auto foregroundForRow = [&model](int row) {
        return model.data(model.index(row, 0), Qt::ForegroundRole)
                .value<QBrush>()
                .color();
    };

    EXPECT_EQ(palette.base(TandaColorCategory::Regular), backgroundForRow(0));
    EXPECT_EQ(palette.base(TandaColorCategory::Vals), backgroundForRow(1));
    EXPECT_EQ(palette.base(TandaColorCategory::Vals), backgroundForRow(2));
    EXPECT_EQ(palette.base(TandaColorCategory::Vals), backgroundForRow(3));
    EXPECT_EQ(TandaColorPalette::autoTextColor(backgroundForRow(1)),
            foregroundForRow(1));

    PerformanceRegistry::instance().mark(special);
    EXPECT_EQ(palette.base(TandaColorCategory::Performance), backgroundForRow(4));
    // The model keeps a deterministic fallback if marks somehow coexist even
    // though the menu normally makes them mutually exclusive.
    CortinaRegistry::instance().mark(special);
    EXPECT_EQ(palette.base(TandaColorCategory::Cortina), backgroundForRow(4));

    CortinaRegistry::instance().unmark(special);
    PerformanceRegistry::instance().unmark(special);
}

TEST_F(TandaQueueDaoTest, RemoveHeaderExpandsToTandaMembers) {
    PlaylistDAO& dao = internalCollection()->getPlaylistDAO();

    const TrackId a = addTrack(QStringLiteral("artist.mp3"));
    const TrackId b = addTrack(QStringLiteral("cover-test-jpg.mp3"));
    const TrackId c = addTrack(QStringLiteral("cover-test-png.mp3"));
    const TrackId d = addTrack(QStringLiteral("cover-test-vbr.mp3"));
    const TrackId e = addTrack(QStringLiteral("empty.mp3"));
    ASSERT_TRUE(a.isValid());
    ASSERT_TRUE(b.isValid());
    ASSERT_TRUE(c.isValid());
    ASSERT_TRUE(d.isValid());
    ASSERT_TRUE(e.isValid());

    const int collapsedPlaylistId =
            dao.createPlaylist(QStringLiteral("Tanda header remove collapsed"),
                    PlaylistDAO::PLHT_NOT_HIDDEN);
    ASSERT_GE(collapsedPlaylistId, 0);
    ASSERT_TRUE(dao.appendTracksToPlaylist({a, b, c, d, e}, collapsedPlaylistId));

    PlaylistTableModel collapsedSource(
            nullptr, trackCollectionManager(), "tanda_remove_collapsed_test");
    collapsedSource.selectPlaylist(collapsedPlaylistId);
    collapsedSource.select();
    ASSERT_EQ(5, collapsedSource.rowCount());

    TandaQueueState collapsedState{UserSettingsPointer()};
    collapsedState.restore({a, b, c, d, e});
    const QUuid collapsedTanda =
            collapsedState.classify({2, 3, 4}, TandaType::Tango);
    ASSERT_FALSE(collapsedTanda.isNull());
    ASSERT_TRUE(collapsedState.spanById(collapsedTanda)->collapsed);
    collapsedSource.setTandaQueueState(&collapsedState);

    TandaQueueModel collapsedModel(&collapsedSource, &collapsedState);
    ASSERT_EQ(3, collapsedModel.rowCount());
    ASSERT_TRUE(collapsedModel.isHeaderRow(1));

    collapsedModel.removeTracks({collapsedModel.index(1, 0)});
    EXPECT_EQ(QVector<TrackId>({a, e}),
            dao.getTrackIdsInPlaylistOrder(collapsedPlaylistId));
    EXPECT_TRUE(collapsedState.spans().isEmpty());

    const int expandedPlaylistId =
            dao.createPlaylist(QStringLiteral("Tanda header remove expanded"),
                    PlaylistDAO::PLHT_NOT_HIDDEN);
    ASSERT_GE(expandedPlaylistId, 0);
    ASSERT_TRUE(dao.appendTracksToPlaylist({a, b, c, d, e}, expandedPlaylistId));

    PlaylistTableModel expandedSource(
            nullptr, trackCollectionManager(), "tanda_remove_expanded_test");
    expandedSource.selectPlaylist(expandedPlaylistId);
    expandedSource.select();
    ASSERT_EQ(5, expandedSource.rowCount());

    TandaQueueState expandedState{UserSettingsPointer()};
    expandedState.restore({a, b, c, d, e});
    const QUuid expandedTanda =
            expandedState.classify({2, 3, 4}, TandaType::Vals);
    ASSERT_FALSE(expandedTanda.isNull());
    ASSERT_TRUE(expandedState.setCollapsed(expandedTanda, false));
    expandedSource.setTandaQueueState(&expandedState);

    TandaQueueModel expandedModel(&expandedSource, &expandedState);
    ASSERT_EQ(6, expandedModel.rowCount());
    ASSERT_TRUE(expandedModel.isHeaderRow(1));
    EXPECT_EQ(1, expandedModel.mapToSource(expandedModel.index(2, 0)).row());

    // Selecting both a header and one of its children should still remove each
    // source row only once.
    expandedModel.removeTracks(
            {expandedModel.index(1, 0), expandedModel.index(2, 0)});
    EXPECT_EQ(QVector<TrackId>({a, e}),
            dao.getTrackIdsInPlaylistOrder(expandedPlaylistId));
    EXPECT_TRUE(expandedState.spans().isEmpty());
}
