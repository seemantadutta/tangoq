// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#include "semanticstate/tangoqsemanticstateprojector.h"

#include <gtest/gtest.h>

namespace mixxx::semanticstate {

namespace {

Track track(const QString& id, const QString& title) {
    return {id, QStringLiteral("Orquesta"), title, 180000};
}

ProjectionInput makeInput() {
    ProjectionInput input;
    input.sessionId = QStringLiteral("projection-session");
    input.startedAt = QDateTime::fromString(
            QStringLiteral("2026-08-28T12:00:00.000Z"), Qt::ISODateWithMs);
    input.queue = {
            {1, track(QStringLiteral("tangoq:1"), QStringLiteral("One"))},
            {2, track(QStringLiteral("tangoq:2"), QStringLiteral("Two"))},
            {3, track(QStringLiteral("tangoq:3"), QStringLiteral("Cortina"))},
            {4, track(QStringLiteral("tangoq:4"), QStringLiteral("Four"))},
            {5, track(QStringLiteral("tangoq:5"), QStringLiteral("Five"))},
    };
    input.tandas = {
            {QStringLiteral("tanda-1"), QStringLiteral("tango"), QString(), 1, 2},
            {QStringLiteral("tanda-2"), QStringLiteral("vals"), QString(), 4, 2},
    };
    input.cortinaTrackIds.insert(QStringLiteral("tangoq:3"));
    return input;
}

} // namespace

TEST(TangoQSemanticStateProjectorTest, EmptyCursorDoesNotGuessUpcomingState) {
    const State state = projectState(makeInput());
    EXPECT_EQ(QStringLiteral("stopped"), state.playback.state);
    EXPECT_FALSE(state.playback.track);
    EXPECT_FALSE(state.playback.queuePosition);
    EXPECT_FALSE(state.tangoq.currentTanda);
    EXPECT_FALSE(state.tangoq.upcomingTanda);
    EXPECT_EQ(QStringLiteral("none"), state.tangoq.cortina.state);
}

TEST(TangoQSemanticStateProjectorTest, ActiveCursorDefinesAutoDjCurrentTrack) {
    ProjectionInput input = makeInput();
    input.activeQueuePosition = 2;
    input.decks.append({input.queue.at(1).track, true, 4567});

    const State state = projectState(input);
    ASSERT_TRUE(state.playback.track);
    EXPECT_EQ(QStringLiteral("tangoq:2"), state.playback.track->id);
    EXPECT_EQ(QStringLiteral("playing"), state.playback.state);
    ASSERT_TRUE(state.playback.queuePosition);
    EXPECT_EQ(2, *state.playback.queuePosition);
    ASSERT_TRUE(state.playback.positionMs);
    EXPECT_EQ(4567, *state.playback.positionMs);
    ASSERT_TRUE(state.tangoq.currentTanda);
    EXPECT_EQ(2, state.tangoq.currentTanda->trackIndex);
    ASSERT_TRUE(state.tangoq.upcomingTanda);
    EXPECT_EQ(QStringLiteral("tanda-2"), state.tangoq.upcomingTanda->id);
    EXPECT_EQ(QStringLiteral("upcoming"), state.tangoq.cortina.state);
    EXPECT_EQ(3, *state.tangoq.cortina.queuePosition);
}

TEST(TangoQSemanticStateProjectorTest, ActiveLoadedTrackThatIsNotPlayingIsPaused) {
    ProjectionInput input = makeInput();
    input.activeQueuePosition = 1;
    input.decks.append({input.queue.first().track, false, 1000});

    const State state = projectState(input);
    EXPECT_EQ(QStringLiteral("paused"), state.playback.state);
    ASSERT_TRUE(state.playback.positionMs);
    EXPECT_EQ(1000, *state.playback.positionMs);
}

TEST(TangoQSemanticStateProjectorTest, MultipleManualDecksRemainExplicitlyAmbiguous) {
    ProjectionInput input = makeInput();
    input.decks.append({input.queue.at(0).track, true, 1000});
    input.decks.append({input.queue.at(1).track, true, 2000});

    const State state = projectState(input);
    EXPECT_EQ(QStringLiteral("playing"), state.playback.state);
    EXPECT_FALSE(state.playback.track);
    EXPECT_FALSE(state.playback.positionMs);
    EXPECT_FALSE(state.playback.queuePosition);
}

TEST(TangoQSemanticStateProjectorTest, ManualPlaybackDoesNotMasqueradeAsAutoDjCursor) {
    ProjectionInput input = makeInput();
    input.activeQueuePosition = 1;
    input.decks.append({input.queue.at(3).track, true, 2000});

    const State state = projectState(input);
    EXPECT_EQ(QStringLiteral("playing"), state.playback.state);
    ASSERT_TRUE(state.playback.track);
    EXPECT_EQ(QStringLiteral("tangoq:4"), state.playback.track->id);
    EXPECT_FALSE(state.playback.queuePosition);
    ASSERT_TRUE(state.playback.positionMs);
    EXPECT_EQ(2000, *state.playback.positionMs);
    ASSERT_TRUE(state.tangoq.currentTanda);
    EXPECT_EQ(QStringLiteral("tanda-1"), state.tangoq.currentTanda->tanda.id);
}

TEST(TangoQSemanticStateProjectorTest, CurrentCortinaHasNoCurrentTandaAndPreviewsNextTanda) {
    ProjectionInput input = makeInput();
    input.activeQueuePosition = 3;
    input.decks.append({input.queue.at(2).track, true, 500});

    const State state = projectState(input);
    EXPECT_FALSE(state.tangoq.currentTanda);
    ASSERT_TRUE(state.tangoq.upcomingTanda);
    EXPECT_EQ(QStringLiteral("tanda-2"), state.tangoq.upcomingTanda->id);
    EXPECT_EQ(QStringLiteral("current"), state.tangoq.cortina.state);
    ASSERT_TRUE(state.tangoq.cortina.track);
    EXPECT_EQ(QStringLiteral("tangoq:3"), state.tangoq.cortina.track->id);
}

TEST(TangoQSemanticStateProjectorTest, FinalTrackOfPartialTandaRemainsCurrent) {
    ProjectionInput input = makeInput();
    input.activeQueuePosition = 5;
    input.decks.append({input.queue.at(4).track, true, 999});

    const State state = projectState(input);
    ASSERT_TRUE(state.tangoq.currentTanda);
    EXPECT_EQ(QStringLiteral("tanda-2"), state.tangoq.currentTanda->tanda.id);
    EXPECT_EQ(2, state.tangoq.currentTanda->trackIndex);
    EXPECT_EQ(2, state.tangoq.currentTanda->tanda.trackCount);
    EXPECT_FALSE(state.tangoq.upcomingTanda);
    EXPECT_EQ(QStringLiteral("none"), state.tangoq.cortina.state);
}

} // namespace mixxx::semanticstate
