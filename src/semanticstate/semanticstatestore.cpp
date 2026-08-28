// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#include "semanticstate/semanticstatestore.h"

#include <QJsonDocument>

#include "moc_semanticstatestore.cpp"

namespace mixxx::semanticstate {

Store::Store(QObject* pParent)
        : QObject(pParent) {
}

bool Store::publish(const State& state, const QString& changeType) {
    const QJsonObject nextState = stateToJson(state);
    if (nextState == m_state) {
        return false;
    }
    m_state = nextState;
    ++m_revision;

    const QJsonObject event{
            {QStringLiteral("schemaVersion"), kSchemaVersion},
            {QStringLiteral("type"), QStringLiteral("state.changed")},
            {QStringLiteral("change"), changeType},
            {QStringLiteral("revision"), static_cast<double>(m_revision)},
            {QStringLiteral("snapshot"), snapshot()},
    };
    emit eventPublished(QJsonDocument(event).toJson(QJsonDocument::Compact));
    return true;
}

QJsonObject Store::snapshot() const {
    QJsonObject result = m_state;
    result.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
    result.insert(QStringLiteral("revision"), static_cast<double>(m_revision));
    return result;
}

QByteArray Store::snapshotJson() const {
    return QJsonDocument(snapshot()).toJson(QJsonDocument::Compact);
}

} // namespace mixxx::semanticstate
