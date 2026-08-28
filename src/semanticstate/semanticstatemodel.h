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

inline constexpr int kSchemaVersion = 2;

struct Track {
    QString id;
    QString artist;
    QString title;
    std::optional<qint64> durationMs;
};

struct QueueItem {
    int position{0};
    Track track;
};

struct Playback {
    QString state{QStringLiteral("stopped")};
    std::optional<Track> track;
    std::optional<int> queuePosition;
    std::optional<qint64> positionMs;
};

struct Tanda {
    QString id;
    QString type;
    QString name;
    int startPosition{0};
    int trackCount{0};
};

struct CurrentTanda {
    Tanda tanda;
    int trackIndex{0};
};

struct Cortina {
    QString state{QStringLiteral("none")};
    std::optional<int> queuePosition;
    std::optional<Track> track;
};

struct TangoQExtension {
    QVector<Tanda> tandas;
    std::optional<CurrentTanda> currentTanda;
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
