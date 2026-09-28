// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/trackset/setlogfeature.h"

#include <gtest/gtest.h>

#include <QSignalSpy>

#include "library/dao/playlistdao.h"
#include "test/librarytest.h"
#include "track/track.h"

namespace {

const QString kTrackLocationTest = QStringLiteral("id3-test-data/cover-test-png.mp3");

} // namespace

class SetlogFeatureTest : public LibraryTest {
  protected:
    PlaylistDAO& playlistDao() {
        return internalCollection()->getPlaylistDAO();
    }

    TrackId addTrackToCollection() {
        TrackPointer pTrack =
                getOrAddTrackByLocation(getTestDir().filePath(kTrackLocationTest));
        return pTrack ? pTrack->getId() : TrackId();
    }
};

TEST_F(SetlogFeatureTest, TangoSessionIsNamedByItsStartDateAndTime) {
    // A session is named when TangoQ first plays a track, so the name says
    // when the set started rather than only which day it was.
    const QDateTime start(QDate(2026, 9, 26), QTime(15, 16, 42));
    const int id = SetlogFeature::createTangoSessionPlaylist(&playlistDao(), start);

    ASSERT_NE(kInvalidPlaylistId, id);
    EXPECT_EQ(QStringLiteral("2026-09-26 15:16"), playlistDao().getPlaylistName(id));
    EXPECT_EQ(PlaylistDAO::PLHT_SET_LOG, playlistDao().getHiddenType(id));
}

TEST_F(SetlogFeatureTest, TangoSessionsStartedInTheSameMinuteGetANumber) {
    const QDateTime start(QDate(2026, 9, 26), QTime(15, 16));
    const int first = SetlogFeature::createTangoSessionPlaylist(&playlistDao(), start);
    const int second = SetlogFeature::createTangoSessionPlaylist(
            &playlistDao(), start.addSecs(20));

    ASSERT_NE(kInvalidPlaylistId, first);
    ASSERT_NE(kInvalidPlaylistId, second);
    EXPECT_NE(first, second);
    EXPECT_EQ(QStringLiteral("2026-09-26 15:16 #2"),
            playlistDao().getPlaylistName(second));
}

TEST_F(SetlogFeatureTest, DeletingSeveralSessionsRecalculatesPlayCounts) {
    // Deleting one session already asks the track DAO to recalculate play
    // counts from the history that is left. Deleting several at once (a year
    // node, or the startup cleanup) must do the same, or the counts keep plays
    // whose history is gone.
    const TrackId trackId = addTrackToCollection();
    ASSERT_TRUE(trackId.isValid());
    const QDateTime start(QDate(2026, 9, 26), QTime(15, 16));
    const int first = SetlogFeature::createTangoSessionPlaylist(&playlistDao(), start);
    const int second = SetlogFeature::createTangoSessionPlaylist(
            &playlistDao(), start.addDays(7));
    ASSERT_TRUE(playlistDao().appendTrackToPlaylist(trackId, first));
    ASSERT_TRUE(playlistDao().appendTrackToPlaylist(trackId, second));
    // A regular playlist is not history, so deleting it changes no counts.
    const int regular = playlistDao().createPlaylist(QStringLiteral("Tandas"));
    ASSERT_TRUE(playlistDao().appendTrackToPlaylist(trackId, regular));

    QSignalSpy spy(&playlistDao(), &PlaylistDAO::tracksRemovedFromPlayedHistory);
    ASSERT_TRUE(playlistDao().deletePlaylists(
            {QString::number(first), QString::number(second)}));

    ASSERT_EQ(1, spy.count());
    const auto trackIds = spy.at(0).at(0).value<QSet<TrackId>>();
    EXPECT_EQ(QSet<TrackId>{trackId}, trackIds);

    spy.clear();
    ASSERT_TRUE(playlistDao().deletePlaylists({QString::number(regular)}));
    EXPECT_EQ(0, spy.count());
}
