// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/tandacolorpalette.h"

#include <algorithm>
#include <cmath>

#include "moc_tandacolorpalette.cpp"

namespace {

const QString kConfigGroup = QStringLiteral("[TangoColors]");
constexpr float kPlayingLightnessScale = 1.35F;
constexpr float kPlayingLightnessCeiling = 0.72F;
constexpr float kPlayingSaturationScale = 1.05F;
constexpr float kPlayedLightnessScale = 0.5F;
constexpr float kPlayedLightnessFloor = 0.16F;
constexpr float kPlayedSaturationScale = 0.85F;

qreal linearSrgb(qreal channel) {
    return channel <= 0.04045
            ? channel / 12.92
            : std::pow((channel + 0.055) / 1.055, 2.4);
}

} // namespace

TandaColorState tandaTrackColorState(int oneBasedPosition, int cursor) {
    if (cursor <= 0 || oneBasedPosition > cursor) {
        return TandaColorState::Upcoming;
    }
    return oneBasedPosition == cursor
            ? TandaColorState::Playing
            : TandaColorState::Played;
}

TandaColorState tandaHeaderColorState(
        int oneBasedStart, int memberCount, int cursor) {
    if (cursor <= 0 || cursor < oneBasedStart) {
        return TandaColorState::Upcoming;
    }
    const int end = oneBasedStart + memberCount - 1;
    return cursor <= end
            ? TandaColorState::Playing
            : TandaColorState::Played;
}

TandaColorPalette::TandaColorPalette(
        UserSettingsPointer pConfig, QObject* pParent)
        : QObject(pParent),
          m_pConfig(std::move(pConfig)) {
}

// static
TandaColorPalette* TandaColorPalette::shared(
        const UserSettingsPointer& pConfig) {
    // TangoQ has one UserSettings object per process. Keep a process-lifetime
    // palette so the preferences page and both queue views share notifications.
    static TandaColorPalette* s_pPalette = new TandaColorPalette(pConfig);
    if (!s_pPalette->m_pConfig && pConfig) {
        s_pPalette->m_pConfig = pConfig;
    }
    return s_pPalette;
}

QColor TandaColorPalette::base(TandaColorCategory category) const {
    if (!m_pConfig) {
        return defaultBase(category);
    }
    const QColor configured(
            m_pConfig->getValueString(ConfigKey(kConfigGroup, configKey(category))));
    return configured.isValid() ? configured : defaultBase(category);
}

void TandaColorPalette::setBase(
        TandaColorCategory category, const QColor& color) {
    if (!m_pConfig || !color.isValid() || base(category) == color) {
        return;
    }
    m_pConfig->set(ConfigKey(kConfigGroup, configKey(category)),
            ConfigValue(color.name(QColor::HexRgb)));
    emit changed();
}

// static
QColor TandaColorPalette::defaultBase(TandaColorCategory category) {
    switch (category) {
    case TandaColorCategory::Tango:
        return QColor(QStringLiteral("#3d6fb0"));
    case TandaColorCategory::Vals:
        return QColor(QStringLiteral("#3f9d55"));
    case TandaColorCategory::Milonga:
        return QColor(QStringLiteral("#c08a2e"));
    case TandaColorCategory::NuevoAlternative:
        return QColor(QStringLiteral("#8a5cc0"));
    case TandaColorCategory::Cortina:
        return QColor(QStringLiteral("#6a7480"));
    case TandaColorCategory::Performance:
        return QColor(QStringLiteral("#c85a9a"));
    case TandaColorCategory::Regular:
        return QColor(QStringLiteral("#4a5058"));
    }
    return QColor(QStringLiteral("#4a5058"));
}

// static
QColor TandaColorPalette::resolvedColor(
        const QColor& base, TandaColorState state) {
    if (!base.isValid() || state == TandaColorState::Upcoming) {
        return base;
    }

    float hue = 0.0F;
    float saturation = 0.0F;
    float lightness = 0.0F;
    float alpha = 1.0F;
    base.getHslF(&hue, &saturation, &lightness, &alpha);
    // QColor reports an undefined hue (-1) for greys. A zero-saturation HSL
    // color is hue-independent, but fromHslF still requires a valid range.
    hue = std::max(0.0F, hue);

    switch (state) {
    case TandaColorState::Upcoming:
        break;
    case TandaColorState::Playing:
        lightness = std::min(lightness * kPlayingLightnessScale,
                kPlayingLightnessCeiling);
        saturation = std::min(saturation * kPlayingSaturationScale, 1.0F);
        break;
    case TandaColorState::Played:
        lightness = std::clamp(lightness * kPlayedLightnessScale,
                kPlayedLightnessFloor,
                1.0F);
        saturation *= kPlayedSaturationScale;
        break;
    }
    return QColor::fromHslF(hue, saturation, lightness, alpha);
}

QColor TandaColorPalette::resolved(
        TandaColorCategory category, TandaColorState state) const {
    return resolvedColor(base(category), state);
}

// static
QColor TandaColorPalette::autoTextColor(const QColor& background) {
    if (!background.isValid()) {
        return QColor(0xf0, 0xf0, 0xf0);
    }
    const qreal luminance = 0.2126 * linearSrgb(background.redF()) +
            0.7152 * linearSrgb(background.greenF()) +
            0.0722 * linearSrgb(background.blueF());
    // Pick the higher WCAG contrast ratio. Solving blackContrast >
    // whiteContrast gives this luminance crossover.
    constexpr qreal kBlackWhiteContrastCrossover = 0.179;
    return luminance > kBlackWhiteContrastCrossover
            ? QColor(0x10, 0x10, 0x10)
            : QColor(0xf0, 0xf0, 0xf0);
}

// static
QString TandaColorPalette::configKey(TandaColorCategory category) {
    switch (category) {
    case TandaColorCategory::Tango:
        return QStringLiteral("Tango");
    case TandaColorCategory::Vals:
        return QStringLiteral("Vals");
    case TandaColorCategory::Milonga:
        return QStringLiteral("Milonga");
    case TandaColorCategory::NuevoAlternative:
        return QStringLiteral("NuevoAlternative");
    case TandaColorCategory::Cortina:
        return QStringLiteral("Cortina");
    case TandaColorCategory::Performance:
        return QStringLiteral("Performance");
    case TandaColorCategory::Regular:
        return QStringLiteral("Regular");
    }
    return QStringLiteral("Regular");
}
