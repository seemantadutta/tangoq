// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#include "semanticstate/semanticstatemodel.h"

#include <QJsonArray>

namespace mixxx::semanticstate {

QJsonObject trackToJson(const Track& track) {
    return {
            {QStringLiteral("id"), track.id},
            {QStringLiteral("artist"), track.artist},
            {QStringLiteral("title"), track.title},
            {QStringLiteral("durationMs"), static_cast<double>(track.durationMs)},
    };
}

namespace {

QJsonObject tandaToJson(const Tanda& tanda, bool includeTrackIndex) {
    QJsonObject object{
            {QStringLiteral("id"), tanda.id},
            {QStringLiteral("type"), tanda.type},
            {QStringLiteral("name"), tanda.name},
            {QStringLiteral("startPosition"), tanda.startPosition},
            {QStringLiteral("trackCount"), tanda.trackCount},
    };
    if (includeTrackIndex) {
        object.insert(QStringLiteral("trackIndex"), tanda.trackIndex);
    }
    return object;
}

} // namespace

QJsonObject stateToJson(const State& state) {
    QJsonObject playback{
            {QStringLiteral("state"), state.playback.state},
            {QStringLiteral("queuePosition"), state.playback.queuePosition},
            {QStringLiteral("positionMs"), static_cast<double>(state.playback.positionMs)},
            {QStringLiteral("durationMs"), static_cast<double>(state.playback.durationMs)},
    };
    if (state.playback.track) {
        playback.insert(QStringLiteral("track"), trackToJson(*state.playback.track));
    } else {
        playback.insert(QStringLiteral("track"), QJsonValue::Null);
    }

    QJsonArray queue;
    for (const auto& item : state.queue) {
        queue.append(QJsonObject{
                {QStringLiteral("position"), item.position},
                {QStringLiteral("track"), trackToJson(item.track)},
        });
    }

    QJsonArray tandas;
    for (const auto& tanda : state.tangoq.tandas) {
        tandas.append(tandaToJson(tanda, false));
    }

    QJsonObject tangoq{
            {QStringLiteral("tandas"), tandas},
    };
    tangoq.insert(QStringLiteral("currentTanda"),
            state.tangoq.currentTanda
                    ? QJsonValue(tandaToJson(*state.tangoq.currentTanda, true))
                    : QJsonValue(QJsonValue::Null));
    tangoq.insert(QStringLiteral("upcomingTanda"),
            state.tangoq.upcomingTanda
                    ? QJsonValue(tandaToJson(*state.tangoq.upcomingTanda, false))
                    : QJsonValue(QJsonValue::Null));

    QJsonObject cortina{
            {QStringLiteral("state"), state.tangoq.cortina.state},
            {QStringLiteral("queuePosition"), state.tangoq.cortina.queuePosition},
    };
    cortina.insert(QStringLiteral("track"),
            state.tangoq.cortina.track
                    ? QJsonValue(trackToJson(*state.tangoq.cortina.track))
                    : QJsonValue(QJsonValue::Null));
    tangoq.insert(QStringLiteral("cortina"), cortina);

    return {
            {QStringLiteral("session"),
                    QJsonObject{
                            {QStringLiteral("id"), state.sessionId},
                            {QStringLiteral("startedAt"),
                                    state.startedAt.toUTC().toString(Qt::ISODateWithMs)},
                    }},
            {QStringLiteral("playback"), playback},
            {QStringLiteral("queue"), queue},
            {QStringLiteral("extensions"),
                    QJsonObject{{QStringLiteral("tangoq"), tangoq}}},
    };
}

} // namespace mixxx::semanticstate
