// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QString>
#include <algorithm>
#include <cmath>

/// The cortina level: a volume offset in whole dB applied to cortinas on the
/// main output, relative to the level each file plays at. Shared by the
/// processor, the Auto DJ toolbar and Preferences so they agree on the range.
namespace mixxx::cortinalevel {

// Provisional range, wide enough to hear the whole scale. Boosting can clip a
// cortina that is already mastered loud, since the main output has no limiter.
constexpr int kMinDb = -12;
constexpr int kMaxDb = 6;
constexpr int kDefaultDb = 0;

inline int clampDb(double db) {
    return std::clamp(static_cast<int>(std::lround(db)), kMinDb, kMaxDb);
}

/// The amplitude multiplier for a level in dB: 10^(dB / 20).
inline double gainForDb(int db) {
    return std::pow(10.0, db / 20.0);
}

/// A level as shown in the UI, e.g. "−4 dB", "0 dB" or "+2 dB", with a true
/// minus sign.
inline QString formatDb(int db) {
    if (db < 0) {
        return QStringLiteral("−%1 dB").arg(-db);
    }
    if (db > 0) {
        return QStringLiteral("+%1 dB").arg(db);
    }
    return QStringLiteral("0 dB");
}

} // namespace mixxx::cortinalevel
