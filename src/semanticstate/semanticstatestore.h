// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>

#include "semanticstate/semanticstatemodel.h"

namespace mixxx::semanticstate {

class Store final : public QObject {
    Q_OBJECT

  public:
    explicit Store(QObject* pParent = nullptr);

    bool publish(const State& state);
    quint64 revision() const {
        return m_revision;
    }
    QJsonObject snapshot() const;
    QByteArray snapshotJson() const;

  signals:
    void eventPublished(const QByteArray& eventJson);

  private:
    QJsonObject m_state;
    quint64 m_revision{0};
};

} // namespace mixxx::semanticstate
