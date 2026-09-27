#pragma once

#include <QString>

#include "preferences/usersettings.h"
#include "util/db/dbconnectionpool.h"

class QSqlDatabase;

class MixxxDb : public QObject {
    Q_OBJECT

  public:
    static const QString kDefaultSchemaFile;

    static const int kRequiredSchemaVersion;

    static const QString kDefaultFileName;

    static bool initDatabaseSchema(
            const QSqlDatabase& database,
            int schemaVersion = kRequiredSchemaVersion,
            const QString& schemaFile = kDefaultSchemaFile);

    /// TangoQ's own tables, versioned separately from the stock schema.
    static const QString kTangoQSchemaFile;
    static const int kRequiredTangoQSchemaVersion;

    /// Creates or upgrades TangoQ's own tables. Call after initDatabaseSchema().
    ///
    /// Before changing anything it writes a copy of the database to
    /// backupFilePath, unless that file already exists (so the copy from
    /// before the first upgrade is kept). Pass an empty path to skip it, e.g.
    /// for an in-memory database. Without a backup, nothing is upgraded.
    ///
    /// Returns false if the tables are not usable at this version. A failed
    /// upgrade is rolled back, so the database is left as it was and the
    /// caller can carry on without the TangoQ features that need them.
    static bool initTangoQSchema(
            const QSqlDatabase& database,
            const QString& backupFilePath,
            int schemaVersion = kRequiredTangoQSchemaVersion,
            const QString& schemaFile = kTangoQSchemaFile);

    explicit MixxxDb(
            const UserSettingsPointer& pConfig,
            bool inMemoryConnection = false);

    mixxx::DbConnectionPoolPtr connectionPool() const {
        return m_pDbConnectionPool;
    }

  private:
    mixxx::DbConnectionPoolPtr m_pDbConnectionPool;
};
