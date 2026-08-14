#pragma once

#include <QString>

#include <cstdlib>

#include "audio/frame.h"

namespace mixxx::tango {

inline QString authoredStartCueLabel() {
    return QStringLiteral("Tango Start");
}

inline bool isAuthoredStartCueLabel(const QString& label) {
    return label == authoredStartCueLabel();
}

// Cue positions can be quantized or come from different analysis passes. Keep
// the equality test in one place so UI, controls, and Auto DJ agree.
constexpr int kStartPositionEqualityToleranceFrames = 2048;

inline bool startPositionsEqual(
        const audio::FramePos& first, const audio::FramePos& second) {
    return first.isValid() && second.isValid() &&
            std::llabs(first.value() - second.value()) <=
            kStartPositionEqualityToleranceFrames;
}

struct StartCueClassification {
    audio::FramePos fas;
    audio::FramePos explicitStart;

    bool hasFas() const {
        return fas.isValid();
    }

    bool hasExplicitStart() const {
        return explicitStart.isValid();
    }
};

enum class EffectiveStartSource {
    ExplicitStart,
    FirstAudibleSound,
    FileStart,
};

struct EffectiveStart {
    audio::FramePos position;
    EffectiveStartSource source;
};

// The label is the durable authorship bit. For old unlabeled files, a valid
// Intro that differs from FAS is retained as an authored start point.
inline StartCueClassification classifyStartCue(
        const audio::FramePos& introPosition,
        const QString& introLabel,
        const audio::FramePos& fasPosition) {
    StartCueClassification result{fasPosition, audio::FramePos()};
    if (!introPosition.isValid()) {
        return result;
    }
    if (isAuthoredStartCueLabel(introLabel) ||
            !startPositionsEqual(introPosition, fasPosition)) {
        result.explicitStart = introPosition;
    }
    return result;
}

inline EffectiveStart tangoEffectiveStart(
        const StartCueClassification& classification) {
    if (classification.hasExplicitStart()) {
        return {classification.explicitStart, EffectiveStartSource::ExplicitStart};
    }
    if (classification.hasFas()) {
        return {classification.fas, EffectiveStartSource::FirstAudibleSound};
    }
    return {audio::kStartFramePos, EffectiveStartSource::FileStart};
}

inline audio::FramePos tangoPlaybackStart(
        const StartCueClassification& classification) {
    return tangoEffectiveStart(classification).position;
}

} // namespace mixxx::tango
