// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/tandacolorpalette.h"

#include <cmath>

#include "moc_tandacolorpalette.cpp"

namespace {

const QString kConfigGroup = QStringLiteral("[TangoColors]");

// The 1.0.2 defaults. "Reset to defaults" used to store them explicitly, so a
// stored copy means "use the default" rather than a deliberate choice.
QColor legacyDefaultBase(TandaColorCategory category) {
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

qreal linearSrgb(qreal channel) {
    return channel <= 0.04045
            ? channel / 12.92
            : std::pow((channel + 0.055) / 1.055, 2.4);
}

} // namespace

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

bool TandaColorPalette::colorCodingEnabled() const {
    return !m_pConfig ||
            m_pConfig->getValue(
                    ConfigKey(kConfigGroup, QStringLiteral("Enabled")), true);
}

void TandaColorPalette::setColorCodingEnabled(bool enabled) {
    if (!m_pConfig || colorCodingEnabled() == enabled) {
        return;
    }
    m_pConfig->set(
            ConfigKey(kConfigGroup, QStringLiteral("Enabled")), ConfigValue(enabled));
    emit changed();
}

QColor TandaColorPalette::base(TandaColorCategory category) const {
    if (!m_pConfig) {
        return defaultBase(category);
    }
    const QColor configured(
            m_pConfig->getValueString(ConfigKey(kConfigGroup, configKey(category))));
    if (!configured.isValid() || configured == legacyDefaultBase(category)) {
        return defaultBase(category);
    }
    return configured;
}

void TandaColorPalette::setBase(
        TandaColorCategory category, const QColor& color) {
    if (!m_pConfig || !color.isValid() || base(category) == color) {
        return;
    }
    const ConfigKey key(kConfigGroup, configKey(category));
    if (color == defaultBase(category)) {
        // Store only customizations, so future default changes still apply.
        m_pConfig->remove(key);
    } else {
        m_pConfig->set(key, ConfigValue(color.name(QColor::HexRgb)));
    }
    emit changed();
}

// static
QColor TandaColorPalette::defaultBase(TandaColorCategory category) {
    switch (category) {
    case TandaColorCategory::Tango:
        return QColor(QStringLiteral("#35507a"));
    case TandaColorCategory::Vals:
        return QColor(QStringLiteral("#3d6b4c"));
    case TandaColorCategory::Milonga:
        return QColor(QStringLiteral("#856a35"));
    case TandaColorCategory::NuevoAlternative:
        return QColor(QStringLiteral("#5f4d80"));
    case TandaColorCategory::Cortina:
        return QColor(QStringLiteral("#4c535b"));
    case TandaColorCategory::Performance:
        return QColor(QStringLiteral("#7d4866"));
    case TandaColorCategory::Regular:
        return QColor(QStringLiteral("#4a5058"));
    }
    return QColor(QStringLiteral("#4a5058"));
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
