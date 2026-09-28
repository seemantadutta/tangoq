// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/tangoplay.h"

#include <gtest/gtest.h>

#include <QColor>

#include "control/controlobject.h"
#include "control/controlpotmeter.h"
#include "library/autodj/cortinaregistry.h"
#include "library/autodj/performanceregistry.h"
#include "library/autodj/tandaqueuestate.h"
#include "library/dao/playlistdao.h"
#include "library/playlisttablemodel.h"
#include "library/trackset/setlogfeature.h"
#include "mixer/playerinfo.h"
#include "test/librarytest.h"
#include "track/track.h"

// Each play TangoQ logs to history is saved as it happened: its role, LIVE,
// and its tanda. The history then shows it that way even after the marks or
// the tandas change.

namespace {

const QString kFirstTrack = QStringLiteral("id3-test-data/cover-test-png.mp3");
const QString kSecondTrack = QStringLiteral("id3-test-data/cover-test-jpg.mp3");
const QString kMasterGroup = QStringLiteral("[Master]");
const QString kAppGroup = QStringLiteral("[App]");

} // namespace

class TangoPlayTest : public LibraryTest {
  protected:
    // The history view test builds a PlaylistTableModel, which needs PlayerInfo
    // and the controls its timer polls. Without them a Debug build trips a
    // DEBUG_ASSERT (a ctest INTERRUPT in the coverage job). See
    // TandaQueueDaoTest for the full explanation.
    TangoPlayTest()
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
    ~TangoPlayTest() override {
        PlayerInfo::destroy();
    }

    void TearDown() override {
        // The registries are process-wide singletons: leave them as found.
        for (const TrackPointer& pTrack : {m_pFirst, m_pSecond}) {
            if (pTrack) {
                CortinaRegistry::instance().unmark(pTrack->getId());
                PerformanceRegistry::instance().unmark(pTrack->getId());
            }
        }
    }

    void SetUp() override {
        m_pFirst = getOrAddTrackByLocation(getTestDir().filePath(kFirstTrack));
        m_pSecond = getOrAddTrackByLocation(getTestDir().filePath(kSecondTrack));
        ASSERT_TRUE(m_pFirst && m_pSecond);
    }

    PlaylistDAO& playlistDao() {
        return internalCollection()->getPlaylistDAO();
    }

    QSqlDatabase database() const {
        return internalCollection()->database();
    }

    int newSession(const QDateTime& start) {
        return SetlogFeature::createTangoSessionPlaylist(&playlistDao(), start);
    }

    // Logs a play the way history does: append the row, then save the play.
    void logPlay(int playlistId, const TangoPlay& play) {
        ASSERT_TRUE(playlistDao().appendTrackToPlaylist(play.track->getId(), playlistId));
        ASSERT_TRUE(tangoplay::save(database(), playlistId, play));
    }

    ControlPotmeter m_crossfader;
    ControlObject m_numDecks;
    ControlObject m_numSamplers;
    ControlObject m_numPreviewDecks;
    TrackPointer m_pFirst;
    TrackPointer m_pSecond;
};

TEST_F(TangoPlayTest, DescribesTheRoleFromTheCurrentMarks) {
    EXPECT_EQ(TangoPlayRole::Regular,
            tangoplay::describe(m_pFirst, 1, nullptr, false).role);

    CortinaRegistry::instance().mark(m_pFirst->getId());
    EXPECT_EQ(TangoPlayRole::Cortina,
            tangoplay::describe(m_pFirst, 1, nullptr, false).role);

    PerformanceRegistry::instance().mark(m_pSecond->getId());
    const TangoPlay play = tangoplay::describe(m_pSecond, 2, nullptr, true);
    EXPECT_EQ(TangoPlayRole::Performance, play.role);
    EXPECT_TRUE(play.live);
    EXPECT_FALSE(play.inTanda());
}

TEST_F(TangoPlayTest, DescribesTheTandaTheTrackIsPartOf) {
    // Queue: a cortina, then a named three-track vals tanda.
    const TrackId cortina(QVariant(90));
    const TrackId a(QVariant(91));
    const TrackId b(QVariant(92));
    const TrackId c(QVariant(93));
    TandaQueueState state(config());
    state.restore({cortina, a, b, c});
    const QUuid tandaId = state.classify({2, 3, 4}, TandaType::Vals);
    ASSERT_FALSE(tandaId.isNull());
    ASSERT_TRUE(state.setName(tandaId, QStringLiteral("Biagi valses")));

    const TangoPlay second = tangoplay::describe(m_pFirst, 3, &state, false);
    EXPECT_EQ(tandaId, second.tandaId);
    EXPECT_EQ(static_cast<int>(TandaType::Vals), second.tandaType);
    EXPECT_EQ(QStringLiteral("Biagi valses"), second.tandaName);
    EXPECT_EQ(2, second.tandaPosition);
    EXPECT_EQ(3, second.tandaSize);

    EXPECT_FALSE(tangoplay::describe(m_pFirst, 1, &state, false).inTanda());
}

TEST_F(TangoPlayTest, TheHistoryKeepsThePlayAsItHappened) {
    const int session = newSession(QDateTime(QDate(2026, 9, 27), QTime(21, 0)));
    ASSERT_NE(kInvalidPlaylistId, session);

    TangoPlay tanda;
    tanda.track = m_pFirst;
    tanda.live = true;
    tanda.tandaId = QUuid::createUuid();
    tanda.tandaType = static_cast<int>(TandaType::Tango);
    tanda.tandaName = QStringLiteral("Di Sarli");
    tanda.tandaPosition = 1;
    tanda.tandaSize = 4;
    logPlay(session, tanda);
    TangoPlay cortina;
    cortina.track = m_pSecond;
    cortina.role = TangoPlayRole::Cortina;
    logPlay(session, cortina);

    // Unmarking or regrouping later does not rewrite what was played.
    const QHash<int, TangoPlay> plays = tangoplay::load(database(), session);
    ASSERT_EQ(2, plays.size());
    EXPECT_EQ(TangoPlayRole::Regular, plays.value(1).role);
    EXPECT_TRUE(plays.value(1).live);
    EXPECT_EQ(tanda.tandaId, plays.value(1).tandaId);
    EXPECT_EQ(QStringLiteral("Di Sarli"), plays.value(1).tandaName);
    EXPECT_EQ(1, plays.value(1).tandaPosition);
    EXPECT_EQ(4, plays.value(1).tandaSize);
    EXPECT_EQ(TangoPlayRole::Cortina, plays.value(2).role);
    EXPECT_FALSE(plays.value(2).live);
    EXPECT_FALSE(plays.value(2).inTanda());
}

TEST_F(TangoPlayTest, JoiningSessionsKeepsTheirPlays) {
    // "Join with previous" copies the rows to the older session, at new
    // positions. Each play must still find its row.
    const int earlier = newSession(QDateTime(QDate(2026, 9, 27), QTime(20, 0)));
    const int later = newSession(QDateTime(QDate(2026, 9, 27), QTime(22, 0)));
    TangoPlay first;
    first.track = m_pFirst;
    logPlay(earlier, first);
    TangoPlay performance;
    performance.track = m_pSecond;
    performance.role = TangoPlayRole::Performance;
    logPlay(later, performance);

    ASSERT_TRUE(playlistDao().copyPlaylistTracks(later, earlier));

    const QHash<int, TangoPlay> plays = tangoplay::load(database(), earlier);
    ASSERT_EQ(2, plays.size());
    EXPECT_EQ(TangoPlayRole::Regular, plays.value(1).role);
    EXPECT_EQ(TangoPlayRole::Performance, plays.value(2).role);
}

TEST_F(TangoPlayTest, OlderHistoryHasNoSavedPlays) {
    // Rows logged before TangoQ saved plays simply have none.
    const int session = newSession(QDateTime(QDate(2026, 9, 20), QTime(21, 0)));
    ASSERT_TRUE(playlistDao().appendTrackToPlaylist(m_pFirst->getId(), session));

    EXPECT_TRUE(tangoplay::load(database(), session).isEmpty());
}

TEST_F(TangoPlayTest, TheHistoryViewShowsHowEachRowWasPlayed) {
    const int session = newSession(QDateTime(QDate(2026, 9, 27), QTime(21, 0)));
    TangoPlay regular;
    regular.track = m_pFirst;
    logPlay(session, regular);
    TangoPlay cortina;
    cortina.track = m_pSecond;
    cortina.role = TangoPlayRole::Cortina;
    logPlay(session, cortina);
    // The cortina has since been unmarked: the history still shows it.
    CortinaRegistry::instance().unmark(m_pSecond->getId());

    PlaylistTableModel model(nullptr, trackCollectionManager(), "mixxx.db.model.setlog", true);
    model.selectPlaylist(session);
    model.select();
    ASSERT_EQ(2, model.rowCount());

    // Unit tests have no library track source, so the model has only its own
    // columns here (no Title, so the title tag is checked by hand). The colour
    // applies to every column of the row.
    const int positionColumn =
            model.fieldIndex(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_POSITION);
    ASSERT_GE(positionColumn, 0);
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex cell = model.index(row, positionColumn);
        const QVariant color = cell.data(Qt::ForegroundRole);
        if (cell.data().toInt() == 2) {
            EXPECT_EQ(QColor(0x33, 0x88, 0xff), color.value<QColor>());
        } else {
            EXPECT_FALSE(color.isValid());
        }
    }
}
