// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#include "semanticstate/semanticstatestore.h"

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>

namespace mixxx::semanticstate {

namespace {

Track makeTrack() {
    return {
            QStringLiteral("tangoq:42"),
            QStringLiteral("Orquesta"),
            QStringLiteral("Tango"),
            180000,
    };
}

State makeState() {
    State state;
    state.sessionId = QStringLiteral("session-1");
    state.startedAt = QDateTime::fromString(
            QStringLiteral("2026-08-28T12:00:00.000Z"), Qt::ISODateWithMs);
    const Track track = makeTrack();
    state.playback.state = QStringLiteral("playing");
    state.playback.track = track;
    state.playback.queuePosition = 1;
    state.playback.positionMs = 1234;
    state.queue.append({1, track});
    state.tangoq.tandas.append({QStringLiteral("tanda-1"),
            QStringLiteral("tango"),
            QStringLiteral("Opening"),
            1,
            4});
    state.tangoq.currentTanda = CurrentTanda{state.tangoq.tandas.first(), 1};
    return state;
}

} // namespace

TEST(SemanticStateStoreTest, EmptyStateIsCompleteAndUsesSchemaVersion) {
    Store store;
    State state;
    state.sessionId = QStringLiteral("session-empty");
    state.startedAt = QDateTime::fromString(
            QStringLiteral("2026-08-28T12:00:00.000Z"), Qt::ISODateWithMs);

    ASSERT_TRUE(store.publish(state));
    const QJsonObject snapshot = store.snapshot();
    EXPECT_EQ(kSchemaVersion, snapshot.value(QStringLiteral("schemaVersion")).toInt());
    EXPECT_EQ(1, snapshot.value(QStringLiteral("revision")).toInt());
    EXPECT_TRUE(snapshot.value(QStringLiteral("queue")).toArray().isEmpty());
    const QJsonObject playback = snapshot.value(QStringLiteral("playback")).toObject();
    EXPECT_EQ(QStringLiteral("stopped"), playback.value(QStringLiteral("state")).toString());
    EXPECT_TRUE(playback.value(QStringLiteral("track")).isNull());
    EXPECT_TRUE(playback.value(QStringLiteral("queuePosition")).isNull());
    EXPECT_TRUE(playback.value(QStringLiteral("positionMs")).isNull());
}

TEST(SemanticStateStoreTest, SnapshotUsesGenericAndExtensionNamespaces) {
    Store store;
    ASSERT_TRUE(store.publish(makeState()));

    const QJsonObject snapshot = store.snapshot();
    EXPECT_EQ(kSchemaVersion, snapshot.value(QStringLiteral("schemaVersion")).toInt());
    EXPECT_EQ(1, snapshot.value(QStringLiteral("revision")).toInt());
    EXPECT_EQ(QStringLiteral("session-1"),
            snapshot.value(QStringLiteral("session"))
                    .toObject()
                    .value(QStringLiteral("id"))
                    .toString());
    EXPECT_EQ(QStringLiteral("playing"),
            snapshot.value(QStringLiteral("playback"))
                    .toObject()
                    .value(QStringLiteral("state"))
                    .toString());
    EXPECT_EQ(1, snapshot.value(QStringLiteral("queue")).toArray().size());
    const QJsonObject tangoq = snapshot.value(QStringLiteral("extensions"))
                                       .toObject()
                                       .value(QStringLiteral("tangoq"))
                                       .toObject();
    EXPECT_EQ(1, tangoq.value(QStringLiteral("tandas")).toArray().size());
    EXPECT_EQ(1,
            tangoq.value(QStringLiteral("currentTanda"))
                    .toObject()
                    .value(QStringLiteral("trackIndex"))
                    .toInt());
}

TEST(SemanticStateStoreTest, RevisionAdvancesOnlyForChangedState) {
    Store store;
    QSignalSpy eventSpy(&store, &Store::eventPublished);
    State state = makeState();

    EXPECT_TRUE(store.publish(state));
    EXPECT_FALSE(store.publish(state));
    EXPECT_EQ(1U, store.revision());
    ASSERT_EQ(1, eventSpy.size());

    state.playback.positionMs = 1500;
    EXPECT_TRUE(store.publish(state));
    EXPECT_EQ(2U, store.revision());
    ASSERT_EQ(2, eventSpy.size());

    const QByteArray bytes = eventSpy.at(1).at(0).toByteArray();
    const QJsonObject event = QJsonDocument::fromJson(bytes).object();
    EXPECT_EQ(2, event.value(QStringLiteral("revision")).toInt());
    EXPECT_EQ(QStringLiteral("state.changed"), event.value(QStringLiteral("type")).toString());
    EXPECT_FALSE(event.contains(QStringLiteral("change")));
    EXPECT_EQ(2,
            event.value(QStringLiteral("snapshot"))
                    .toObject()
                    .value(QStringLiteral("revision"))
                    .toInt());
}

TEST(SemanticStateStoreTest, SessionChangeIsObservableAndRevisionRemainsProcessLocal) {
    Store store;
    State state = makeState();
    ASSERT_TRUE(store.publish(state));

    state.sessionId = QStringLiteral("session-2");
    state.startedAt = state.startedAt.addSecs(1);
    EXPECT_TRUE(store.publish(state));
    EXPECT_EQ(2U, store.revision());
    EXPECT_EQ(QStringLiteral("session-2"),
            store.snapshot()
                    .value(QStringLiteral("session"))
                    .toObject()
                    .value(QStringLiteral("id"))
                    .toString());
}

TEST(SemanticStateStoreTest, NullUnknownsAndUtf8MetadataSerializeSafely) {
    Store store;
    State state = makeState();
    state.playback.track.reset();
    state.playback.queuePosition.reset();
    state.playback.positionMs.reset();
    state.queue.first().track.artist = QString::fromUtf8("An\xC3\xAD" "bal Troilo");
    state.queue.first().track.title = QString::fromUtf8("\xE2\x80\x9CQue nadie sepa mi sufrir\xE2\x80\x9D\n");
    state.queue.first().track.durationMs.reset();
    state.tangoq.currentTanda.reset();
    state.tangoq.upcomingTanda.reset();

    ASSERT_TRUE(store.publish(state));
    const QJsonObject snapshot = QJsonDocument::fromJson(store.snapshotJson()).object();
    const QJsonObject playback = snapshot.value(QStringLiteral("playback")).toObject();
    EXPECT_TRUE(playback.value(QStringLiteral("track")).isNull());
    EXPECT_TRUE(playback.value(QStringLiteral("queuePosition")).isNull());
    EXPECT_TRUE(playback.value(QStringLiteral("positionMs")).isNull());
    const QJsonObject track = snapshot.value(QStringLiteral("queue"))
                                      .toArray()
                                      .first()
                                      .toObject()
                                      .value(QStringLiteral("track"))
                                      .toObject();
    EXPECT_EQ(QString::fromUtf8("An\xC3\xAD" "bal Troilo"),
            track.value(QStringLiteral("artist")).toString());
    EXPECT_TRUE(track.value(QStringLiteral("durationMs")).isNull());
}

TEST(SemanticStateStoreTest, SnapshotJsonMatchesSnapshotObject) {
    Store store;
    ASSERT_TRUE(store.publish(makeState()));
    EXPECT_EQ(store.snapshot(), QJsonDocument::fromJson(store.snapshotJson()).object());
}

} // namespace mixxx::semanticstate
