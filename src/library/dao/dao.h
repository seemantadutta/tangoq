#pragma once

#include <QSqlDatabase>

#include "util/assert.h"

class DAO {
  public:
    virtual ~DAO() = default;

    virtual void initialize(const QSqlDatabase& database) {
        DEBUG_ASSERT(!m_database.isOpen());
        m_database = database;
    }

    /// Drops this DAO's reference to the connection, without touching the
    /// database itself. QSqlDatabase is implicitly shared, so a DAO that
    /// outlives whoever owns the connection keeps it alive: removing it then
    /// warns that the connection "is still in use, all queries will cease to
    /// work". Call this before the owner hands the connection back.
    virtual void disconnectDatabase() {
        m_database = QSqlDatabase();
    }

    const QSqlDatabase& database() const {
        return m_database;
    }

  protected:
    QSqlDatabase m_database;
};
