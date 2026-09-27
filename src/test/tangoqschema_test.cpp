// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include <gtest/gtest.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

#include "database/mixxxdb.h"
#include "database/schemamanager.h"
#include "library/dao/settingsdao.h"
#include "test/mixxxdbtest.h"
#include "track/trackid.h"

// TangoQ keeps its own tables in tangoq.db, versioned separately from the
// stock Mixxx schema, so an older TangoQ (which only knows the Mixxx schema)
// can still open the database.

namespace {

const QString kVersionKey = QStringLiteral("tangoq.schema.version");

bool tableExists(const QSqlDatabase& database, const QString& table) {
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type='table' AND name=:name"));
    query.bindValue(QStringLiteral(":name"), table);
    return query.exec() && query.next();
}

int count(const QSqlDatabase& database, const QString& sql) {
    QSqlQuery query(database);
    if (!query.exec(sql) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

} // namespace

class TangoQSchemaTest : public MixxxDbTest {
  protected:
    TangoQSchemaTest()
            // A file database, so the backup has something to copy.
            : MixxxDbTest(/*inMemoryDbConnection*/ false) {
    }

    void SetUp() override {
        ASSERT_TRUE(MixxxDb::initDatabaseSchema(dbConnection()));
    }

    QString backupPath() const {
        return m_backupDir.filePath(QStringLiteral("tangoq-backup.db"));
    }

    QTemporaryDir m_backupDir;
};

TEST_F(TangoQSchemaTest, CreatesTheTangoQTablesWithTheirOwnVersion) {
    ASSERT_FALSE(tableExists(dbConnection(), QStringLiteral("tangoq_cortina")));

    EXPECT_TRUE(MixxxDb::initTangoQSchema(dbConnection(), backupPath()));

    EXPECT_TRUE(tableExists(dbConnection(), QStringLiteral("tangoq_cortina")));
    EXPECT_TRUE(tableExists(dbConnection(), QStringLiteral("tangoq_play")));
    const SettingsDAO settings(dbConnection());
    EXPECT_EQ(QString::number(MixxxDb::kRequiredTangoQSchemaVersion),
            settings.getValue(kVersionKey));
    // The stock schema version is left alone.
    EXPECT_EQ(MixxxDb::kRequiredSchemaVersion,
            SchemaManager(dbConnection()).readCurrentVersion());
}

TEST_F(TangoQSchemaTest, BacksUpTheDatabaseBeforeUpgrading) {
    const TrackId trackId = [this] {
        QSqlQuery query(dbConnection());
        query.exec(QStringLiteral(
                "INSERT INTO library (artist, title) VALUES ('Di Sarli', 'Bahía Blanca')"));
        return TrackId(query.lastInsertId());
    }();
    ASSERT_TRUE(trackId.isValid());

    ASSERT_TRUE(MixxxDb::initTangoQSchema(dbConnection(), backupPath()));

    // The backup is the database as it was before the upgrade: the track is
    // there, the TangoQ tables are not.
    ASSERT_TRUE(QFile::exists(backupPath()));
    const QString name = QUuid::createUuid().toString();
    {
        QSqlDatabase backup = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        backup.setDatabaseName(backupPath());
        ASSERT_TRUE(backup.open());
        EXPECT_EQ(1, count(backup, QStringLiteral("SELECT COUNT(*) FROM library")));
        EXPECT_FALSE(tableExists(backup, QStringLiteral("tangoq_cortina")));
        backup.close();
    }
    QSqlDatabase::removeDatabase(name);
}

TEST_F(TangoQSchemaTest, ReopeningIsANoOpAndKeepsTheFirstBackup) {
    ASSERT_TRUE(MixxxDb::initTangoQSchema(dbConnection(), backupPath()));
    ASSERT_TRUE(QFile::exists(backupPath()));
    const QDateTime backupTime = QFileInfo(backupPath()).lastModified();
    QSqlQuery(dbConnection()).exec(QStringLiteral(
            "INSERT INTO tangoq_cortina (track_id) VALUES (7)"));

    EXPECT_TRUE(MixxxDb::initTangoQSchema(dbConnection(), backupPath()));

    EXPECT_EQ(1, count(dbConnection(), QStringLiteral("SELECT COUNT(*) FROM tangoq_cortina")));
    EXPECT_EQ(backupTime, QFileInfo(backupPath()).lastModified());
}

TEST_F(TangoQSchemaTest, AnOlderTangoQCanStillOpenTheDatabase) {
    ASSERT_TRUE(MixxxDb::initTangoQSchema(dbConnection(), backupPath()));

    // TangoQ 1.0.2 only checks the stock schema at startup. That must still
    // find the version it expects, rather than a newer, incompatible one.
    EXPECT_EQ(SchemaManager::Result::CurrentVersion,
            SchemaManager(dbConnection())
                    .upgradeToSchemaVersion(MixxxDb::kRequiredSchemaVersion,
                            MixxxDb::kDefaultSchemaFile));
}

TEST_F(TangoQSchemaTest, ANewerCompatibleTangoQSchemaIsAccepted) {
    ASSERT_TRUE(MixxxDb::initTangoQSchema(dbConnection(), backupPath()));
    // A later TangoQ added a table but stays compatible with this version.
    const SettingsDAO settings(dbConnection());
    settings.setValue(kVersionKey, MixxxDb::kRequiredTangoQSchemaVersion + 1);
    settings.setValue(QStringLiteral("tangoq.schema.min_compatible_version"),
            MixxxDb::kRequiredTangoQSchemaVersion);

    EXPECT_TRUE(MixxxDb::initTangoQSchema(dbConnection(), backupPath()));

    // An incompatible one is refused, without touching the database.
    settings.setValue(QStringLiteral("tangoq.schema.min_compatible_version"),
            MixxxDb::kRequiredTangoQSchemaVersion + 1);
    EXPECT_FALSE(MixxxDb::initTangoQSchema(dbConnection(), backupPath()));
    EXPECT_TRUE(tableExists(dbConnection(), QStringLiteral("tangoq_cortina")));
}

TEST_F(TangoQSchemaTest, AFailedUpgradeLeavesTheDatabaseAsItWas) {
    // A migration whose second statement fails must roll back the first.
    QTemporaryDir dir;
    const QString schemaFile = dir.filePath(QStringLiteral("broken_schema.xml"));
    {
        QFile file(schemaFile);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(R"(<schema>
  <revision version="1" min_compatible="1">
    <description>Broken on purpose.</description>
    <sql>
      CREATE TABLE tangoq_cortina (track_id INTEGER PRIMARY KEY);
      CREATE TABLE this is not sql;
    </sql>
  </revision>
</schema>)");
    }

    EXPECT_FALSE(MixxxDb::initTangoQSchema(dbConnection(),
            backupPath(),
            MixxxDb::kRequiredTangoQSchemaVersion,
            schemaFile));

    EXPECT_FALSE(tableExists(dbConnection(), QStringLiteral("tangoq_cortina")));
    EXPECT_TRUE(SettingsDAO(dbConnection()).getValue(kVersionKey).isNull());
    // The rest of the database still works.
    EXPECT_EQ(0, count(dbConnection(), QStringLiteral("SELECT COUNT(*) FROM library")));
}

TEST_F(TangoQSchemaTest, UpgradesARealDatabase) {
    // Upgrades a copy of a real TangoQ 1.0.2 database. The database holds a
    // DJ's library, so it is never checked in: point TANGOQ_TEST_REAL_DB at a
    // copy to run this locally. The copy is copied again, so it is not changed.
    const QString source = qEnvironmentVariable("TANGOQ_TEST_REAL_DB");
    if (source.isEmpty()) {
        GTEST_SKIP() << "Set TANGOQ_TEST_REAL_DB to a copy of a 1.0.2 tangoq.db";
    }
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("tangoq.db"));
    ASSERT_TRUE(QFile::copy(source, path));

    const QString name = QUuid::createUuid().toString();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(path);
        ASSERT_TRUE(database.open());
        const QString kTracks = QStringLiteral("SELECT COUNT(*) FROM library");
        const QString kPlaylists = QStringLiteral("SELECT COUNT(*) FROM Playlists");
        const QString kHistoryRows = QStringLiteral(
                "SELECT COUNT(*) FROM PlaylistTracks JOIN Playlists "
                "ON PlaylistTracks.playlist_id = Playlists.id WHERE Playlists.hidden = 2");
        const int tracks = count(database, kTracks);
        const int playlists = count(database, kPlaylists);
        const int historyRows = count(database, kHistoryRows);
        ASSERT_GT(tracks, 0);

        ASSERT_TRUE(MixxxDb::initDatabaseSchema(database));
        const QString backup = dir.filePath(QStringLiteral("tangoq-backup.db"));
        EXPECT_TRUE(MixxxDb::initTangoQSchema(database, backup));

        EXPECT_TRUE(tableExists(database, QStringLiteral("tangoq_cortina")));
        EXPECT_TRUE(tableExists(database, QStringLiteral("tangoq_play")));
        EXPECT_EQ(tracks, count(database, kTracks));
        EXPECT_EQ(playlists, count(database, kPlaylists));
        EXPECT_EQ(historyRows, count(database, kHistoryRows));
        QSqlQuery integrity(database);
        ASSERT_TRUE(integrity.exec(QStringLiteral("PRAGMA integrity_check")));
        ASSERT_TRUE(integrity.next());
        EXPECT_EQ(QStringLiteral("ok"), integrity.value(0).toString());
        EXPECT_TRUE(QFile::exists(backup));
        // What 1.0.2 does at startup still succeeds.
        EXPECT_EQ(SchemaManager::Result::CurrentVersion,
                SchemaManager(database).upgradeToSchemaVersion(
                        MixxxDb::kRequiredSchemaVersion, MixxxDb::kDefaultSchemaFile));
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
}
