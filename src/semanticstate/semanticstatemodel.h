// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. See LICENSE for details.

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <optional>

namespace mixxx::semanticstate {

inline constexpr int kSchemaVersion = 1;

struct Track {
    QString id;
    QString artist;
    QString title;
    qint64 durationMs{0};
};

struct QueueItem {
    int position{0};
    Track track;
};

struct Playback {
    QString state{QStringLiteral("stopped")};
    std::optional<Track> track;
    int queuePosition{0};
    qint64 positionMs{0};
    qint64 durationMs{0};
};

struct Tanda {
    QString id;
    QString type;
    QString name;
    int startPosition{0};
    int trackCount{0};
    int trackIndex{0};
};

struct Cortina {
    QString state{QStringLiteral("none")};
    int queuePosition{0};
    std::optional<Track> track;
};

struct TangoQExtension {
    QVector<Tanda> tandas;
    std::optional<Tanda> currentTanda;
    std::optional<Tanda> upcomingTanda;
    Cortina cortina;
};

struct State {
    QString sessionId;
    QDateTime startedAt;
    Playback playback;
    QVector<QueueItem> queue;
    TangoQExtension tangoq;
};

QJsonObject trackToJson(const Track& track);
QJsonObject stateToJson(const State& state);

} // namespace mixxx::semanticstate
