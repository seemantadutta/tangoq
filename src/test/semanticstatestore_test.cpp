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

State makeState() {
    State state;
    state.sessionId = QStringLiteral("session-1");
    state.startedAt = QDateTime::fromString(
            QStringLiteral("2026-08-28T12:00:00.000Z"), Qt::ISODateWithMs);
    const Track track{
            QStringLiteral("tangoq:42"),
            QStringLiteral("Orquesta"),
            QStringLiteral("Tango"),
            180000,
    };
    state.playback = {
            QStringLiteral("playing"), track, 1, 1234, track.durationMs};
    state.queue.append({1, track});
    state.tangoq.tandas.append({QStringLiteral("tanda-1"),
            QStringLiteral("tango"),
            QStringLiteral("Opening"),
            1,
            4,
            0});
    state.tangoq.currentTanda = state.tangoq.tandas.first();
    state.tangoq.currentTanda->trackIndex = 1;
    return state;
}

} // namespace

TEST(SemanticStateStoreTest, SnapshotUsesGenericAndExtensionNamespaces) {
    Store store;
    ASSERT_TRUE(store.publish(makeState(), QStringLiteral("session.started")));

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

    EXPECT_TRUE(store.publish(state, QStringLiteral("session.started")));
    EXPECT_FALSE(store.publish(state, QStringLiteral("playback.positionChanged")));
    EXPECT_EQ(1U, store.revision());
    ASSERT_EQ(1, eventSpy.size());

    state.playback.positionMs = 1500;
    EXPECT_TRUE(store.publish(state, QStringLiteral("playback.positionChanged")));
    EXPECT_EQ(2U, store.revision());
    ASSERT_EQ(2, eventSpy.size());

    const QByteArray bytes = eventSpy.at(1).at(0).toByteArray();
    const QJsonObject event = QJsonDocument::fromJson(bytes).object();
    EXPECT_EQ(2, event.value(QStringLiteral("revision")).toInt());
    EXPECT_EQ(QStringLiteral("playback.positionChanged"),
            event.value(QStringLiteral("change")).toString());
    EXPECT_EQ(2,
            event.value(QStringLiteral("snapshot"))
                    .toObject()
                    .value(QStringLiteral("revision"))
                    .toInt());
}

TEST(SemanticStateStoreTest, SnapshotJsonMatchesSnapshotObject) {
    Store store;
    ASSERT_TRUE(store.publish(makeState(), QStringLiteral("session.started")));
    EXPECT_EQ(store.snapshot(), QJsonDocument::fromJson(store.snapshotJson()).object());
}

} // namespace mixxx::semanticstate
