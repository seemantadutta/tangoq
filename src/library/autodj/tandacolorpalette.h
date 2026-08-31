// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QColor>
#include <QObject>

#include "preferences/usersettings.h"

enum class TandaColorCategory {
    Tango,
    Vals,
    Milonga,
    NuevoAlternative,
    Cortina,
    Performance,
    Regular,
};

enum class TandaColorState {
    Played,
    Playing,
    Upcoming,
};

TandaColorState tandaTrackColorState(int oneBasedPosition, int cursor);
TandaColorState tandaHeaderColorState(
        int oneBasedStart, int memberCount, int cursor);

/// Config-backed colors used by the TangoQ queue.
///
/// Production UI components use shared() so preferences and every queue view
/// observe the same changed() signal. Tests may construct an isolated instance.
class TandaColorPalette final : public QObject {
    Q_OBJECT

  public:
    explicit TandaColorPalette(
            UserSettingsPointer pConfig, QObject* pParent = nullptr);

    static TandaColorPalette* shared(const UserSettingsPointer& pConfig);

    QColor base(TandaColorCategory category) const;
    void setBase(TandaColorCategory category, const QColor& color);

    static QColor defaultBase(TandaColorCategory category);
    static QColor resolvedColor(const QColor& base, TandaColorState state);
    QColor resolved(TandaColorCategory category, TandaColorState state) const;
    static QColor autoTextColor(const QColor& background);

  signals:
    void changed();

  private:
    static QString configKey(TandaColorCategory category);

    UserSettingsPointer m_pConfig;
};
