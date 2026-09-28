// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/cortinaregistry.h"

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QSqlQuery>

#include "test/librarytest.h"

// A cortina mark belongs to the track: it is kept in tangoq.db and comes back
// after a restart, until the DJ unmarks the track.

namespace {

int storedMarks(const QSqlDatabase& database, TrackId trackId) {
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM tangoq_cortina WHERE track_id = :id"));
    query.bindValue(QStringLiteral(":id"), trackId.toVariant());
    if (!query.exec() || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

} // namespace

class CortinaRegistryTest : public LibraryTest {
  protected:
    void TearDown() override {
        // The registry is a process-wide singleton: leave it as found.
        CortinaRegistry::instance().detachDatabase();
        CortinaRegistry::instance().unmark(kCortina);
        CortinaRegistry::instance().unmark(kOtherCortina);
    }

    QSqlDatabase database() const {
        return internalCollection()->database();
    }

    const TrackId kCortina = TrackId(QVariant(101));
    const TrackId kOtherCortina = TrackId(QVariant(102));
};

TEST_F(CortinaRegistryTest, AMarkIsSavedAndRemovedWithTheTrack) {
    CortinaRegistry& registry = CortinaRegistry::instance();
    registry.attachDatabase(database());

    registry.mark(kCortina);
    EXPECT_TRUE(registry.contains(kCortina));
    EXPECT_EQ(1, storedMarks(database(), kCortina));

    registry.unmark(kCortina);
    EXPECT_FALSE(registry.contains(kCortina));
    EXPECT_EQ(0, storedMarks(database(), kCortina));
}

TEST_F(CortinaRegistryTest, MarksComeBackAfterARestart) {
    CortinaRegistry& registry = CortinaRegistry::instance();
    registry.attachDatabase(database());
    registry.mark(kCortina);
    registry.mark(kOtherCortina);
    registry.unmark(kOtherCortina);

    // A restart: the marks in memory are gone, then the database is attached
    // again and the saved marks are loaded.
    registry.detachDatabase();
    registry.unmark(kCortina);
    ASSERT_FALSE(registry.contains(kCortina));
    ASSERT_EQ(1, storedMarks(database(), kCortina)) << "detached: not written";

    QSignalSpy changed(&registry, &CortinaRegistry::cortinaMarksChanged);
    registry.attachDatabase(database());

    EXPECT_TRUE(registry.contains(kCortina));
    EXPECT_FALSE(registry.contains(kOtherCortina));
    EXPECT_EQ(1, changed.count()) << "views repaint once the marks are loaded";
}

TEST_F(CortinaRegistryTest, WithoutTheTableMarksLastForTheSession) {
    // If the TangoQ tables could not be created, marking still works for the
    // session, as it did before marks were saved.
    QSqlQuery(database()).exec(QStringLiteral("DROP TABLE tangoq_cortina"));
    CortinaRegistry& registry = CortinaRegistry::instance();
    registry.attachDatabase(database());

    registry.mark(kCortina);
    EXPECT_TRUE(registry.contains(kCortina));
    registry.unmark(kCortina);
    EXPECT_FALSE(registry.contains(kCortina));
}
