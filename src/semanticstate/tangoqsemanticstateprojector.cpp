// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#include "semanticstate/tangoqsemanticstateprojector.h"

namespace mixxx::semanticstate {

namespace {

bool sameTrack(const std::optional<Track>& track, const QString& id) {
    return track && !id.isEmpty() && track->id == id;
}

Playback projectPlayback(const ProjectionInput& input) {
    Playback playback;

    const DeckState* pOnlyPlaying = nullptr;
    int playingDecks = 0;
    for (const DeckState& deck : input.decks) {
        if (deck.playing && deck.track) {
            ++playingDecks;
            pOnlyPlaying = &deck;
        }
    }

    if (input.activeQueuePosition && *input.activeQueuePosition > 0 &&
            *input.activeQueuePosition <= input.queue.size()) {
        const QueueItem& activeItem = input.queue.at(*input.activeQueuePosition - 1);
        const DeckState* pOnlyMatch = nullptr;
        int matchingDecks = 0;
        for (const DeckState& deck : input.decks) {
            if (!sameTrack(deck.track, activeItem.track.id)) {
                continue;
            }
            ++matchingDecks;
            pOnlyMatch = &deck;
        }

        if (playingDecks == 1) {
            playback.state = QStringLiteral("playing");
            playback.track = pOnlyPlaying->track;
            if (sameTrack(playback.track, activeItem.track.id)) {
                playback.queuePosition = *input.activeQueuePosition;
            }
            playback.positionMs = pOnlyPlaying->positionMs;
        } else if (playingDecks > 1) {
            // There is no authoritative program-track concept for a manual
            // multi-deck mix, even while Auto DJ retains a queue cursor.
            playback.state = QStringLiteral("playing");
        } else if (matchingDecks > 0) {
            playback.state = QStringLiteral("paused");
            playback.track = activeItem.track;
            playback.queuePosition = *input.activeQueuePosition;
            if (matchingDecks == 1) {
                playback.positionMs = pOnlyMatch->positionMs;
            }
        } else {
            playback.state = QStringLiteral("stopped");
        }
        return playback;
    }

    if (playingDecks > 0) {
        playback.state = QStringLiteral("playing");
    }
    if (playingDecks == 1) {
        playback.track = pOnlyPlaying->track;
        playback.positionMs = pOnlyPlaying->positionMs;
    }
    // With no AutoDJ cursor, queue occurrence identity cannot be determined
    // reliably (the same library track may occur more than once).
    return playback;
}

TangoQExtension projectTangoQExtension(const ProjectionInput& input) {
    TangoQExtension extension;
    extension.tandas = input.tandas;

    if (!input.activeQueuePosition || *input.activeQueuePosition <= 0 ||
            *input.activeQueuePosition > input.queue.size()) {
        return extension;
    }

    const int activePosition = *input.activeQueuePosition;
    for (const Tanda& tanda : input.tandas) {
        const int endPosition = tanda.startPosition + tanda.trackCount - 1;
        if (activePosition >= tanda.startPosition && activePosition <= endPosition) {
            extension.currentTanda = CurrentTanda{
                    tanda, activePosition - tanda.startPosition + 1};
        } else if (tanda.startPosition > activePosition && !extension.upcomingTanda) {
            extension.upcomingTanda = tanda;
        }
    }

    const QueueItem& activeItem = input.queue.at(activePosition - 1);
    if (input.cortinaTrackIds.contains(activeItem.track.id)) {
        extension.cortina.state = QStringLiteral("current");
        extension.cortina.queuePosition = activePosition;
        extension.cortina.track = activeItem.track;
        return extension;
    }

    for (int position = activePosition + 1; position <= input.queue.size(); ++position) {
        const QueueItem& item = input.queue.at(position - 1);
        if (input.cortinaTrackIds.contains(item.track.id)) {
            extension.cortina.state = QStringLiteral("upcoming");
            extension.cortina.queuePosition = position;
            extension.cortina.track = item.track;
            break;
        }
    }
    return extension;
}

} // namespace

State projectState(const ProjectionInput& input) {
    return {
            input.sessionId,
            input.startedAt,
            projectPlayback(input),
            input.queue,
            projectTangoQExtension(input),
    };
}

} // namespace mixxx::semanticstate
