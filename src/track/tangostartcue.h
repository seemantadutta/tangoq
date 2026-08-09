#pragma once

#include <QString>

namespace mixxx::tango {

inline QString authoredStartCueLabel() {
    return QStringLiteral("Tango Start");
}

inline bool isAuthoredStartCueLabel(const QString& label) {
    return label == authoredStartCueLabel();
}

} // namespace mixxx::tango
