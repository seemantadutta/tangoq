// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#include "semanticstate/semanticstatemodel.h"

#include <QJsonArray>

namespace mixxx::semanticstate {

QJsonObject trackToJson(const Track& track) {
    QJsonObject object{
            {QStringLiteral("id"), track.id},
            {QStringLiteral("artist"), track.artist},
            {QStringLiteral("title"), track.title},
    };
    object.insert(QStringLiteral("durationMs"),
            track.durationMs
                    ? QJsonValue(static_cast<double>(*track.durationMs))
                    : QJsonValue(QJsonValue::Null));
    return object;
}

namespace {

QJsonObject tandaToJson(const Tanda& tanda) {
    return {
            {QStringLiteral("id"), tanda.id},
            {QStringLiteral("type"), tanda.type},
            {QStringLiteral("name"), tanda.name},
            {QStringLiteral("startPosition"), tanda.startPosition},
            {QStringLiteral("trackCount"), tanda.trackCount},
    };
}

} // namespace

QJsonObject stateToJson(const State& state) {
    QJsonObject playback{
            {QStringLiteral("state"), state.playback.state},
    };
    playback.insert(QStringLiteral("queuePosition"),
            state.playback.queuePosition
                    ? QJsonValue(*state.playback.queuePosition)
                    : QJsonValue(QJsonValue::Null));
    playback.insert(QStringLiteral("positionMs"),
            state.playback.positionMs
                    ? QJsonValue(static_cast<double>(*state.playback.positionMs))
                    : QJsonValue(QJsonValue::Null));
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
        tandas.append(tandaToJson(tanda));
    }

    QJsonObject tangoq{
            {QStringLiteral("tandas"), tandas},
    };
    if (state.tangoq.currentTanda) {
        QJsonObject current = tandaToJson(state.tangoq.currentTanda->tanda);
        current.insert(QStringLiteral("trackIndex"),
                state.tangoq.currentTanda->trackIndex);
        tangoq.insert(QStringLiteral("currentTanda"), current);
    } else {
        tangoq.insert(QStringLiteral("currentTanda"), QJsonValue::Null);
    }
    tangoq.insert(QStringLiteral("upcomingTanda"),
            state.tangoq.upcomingTanda
                    ? QJsonValue(tandaToJson(*state.tangoq.upcomingTanda))
                    : QJsonValue(QJsonValue::Null));

    QJsonObject cortina{
            {QStringLiteral("state"), state.tangoq.cortina.state},
    };
    cortina.insert(QStringLiteral("queuePosition"),
            state.tangoq.cortina.queuePosition
                    ? QJsonValue(*state.tangoq.cortina.queuePosition)
                    : QJsonValue(QJsonValue::Null));
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
