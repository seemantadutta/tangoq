// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/tandacolorpalette.h"

#include <gtest/gtest.h>

#include <QSignalSpy>

#include "test/mixxxtest.h"

namespace {

class TandaColorPaletteTest : public MixxxTest {
};

TEST_F(TandaColorPaletteTest, ShippedDefaultsCoverAllCategories) {
    const TandaColorPalette palette(config());

    EXPECT_EQ(QColor(QStringLiteral("#3d6fb0")),
            palette.base(TandaColorCategory::Tango));
    EXPECT_EQ(QColor(QStringLiteral("#3f9d55")),
            palette.base(TandaColorCategory::Vals));
    EXPECT_EQ(QColor(QStringLiteral("#c08a2e")),
            palette.base(TandaColorCategory::Milonga));
    EXPECT_EQ(QColor(QStringLiteral("#8a5cc0")),
            palette.base(TandaColorCategory::NuevoAlternative));
    EXPECT_EQ(QColor(QStringLiteral("#6a7480")),
            palette.base(TandaColorCategory::Cortina));
    EXPECT_EQ(QColor(QStringLiteral("#c85a9a")),
            palette.base(TandaColorCategory::Performance));
    EXPECT_EQ(QColor(QStringLiteral("#4a5058")),
            palette.base(TandaColorCategory::Regular));
}

TEST_F(TandaColorPaletteTest, ConfigRoundTripPersistsAndSignals) {
    TandaColorPalette palette(config());
    QSignalSpy changedSpy(&palette, &TandaColorPalette::changed);

    const QColor custom(QStringLiteral("#123456"));
    palette.setBase(TandaColorCategory::Vals, custom);
    EXPECT_EQ(1, changedSpy.count());
    EXPECT_EQ(custom, palette.base(TandaColorCategory::Vals));

    // Writing the same value is a no-op.
    palette.setBase(TandaColorCategory::Vals, custom);
    EXPECT_EQ(1, changedSpy.count());

    saveAndReloadConfig();
    const TandaColorPalette restored(config());
    EXPECT_EQ(custom, restored.base(TandaColorCategory::Vals));
}

TEST_F(TandaColorPaletteTest, InvalidConfigFallsBackToDefault) {
    config()->set(ConfigKey(QStringLiteral("[TangoColors]"),
                          QStringLiteral("Milonga")),
            ConfigValue(QStringLiteral("not-a-color")));
    const TandaColorPalette palette(config());

    EXPECT_EQ(TandaColorPalette::defaultBase(TandaColorCategory::Milonga),
            palette.base(TandaColorCategory::Milonga));
}

TEST_F(TandaColorPaletteTest, BrightnessStatesPreserveHueAndOrderLightness) {
    const QColor base(QStringLiteral("#3d6fb0"));
    const QColor played = TandaColorPalette::resolvedColor(
            base, TandaColorState::Played);
    const QColor playing = TandaColorPalette::resolvedColor(
            base, TandaColorState::Playing);
    const QColor upcoming = TandaColorPalette::resolvedColor(
            base, TandaColorState::Upcoming);

    EXPECT_EQ(base, upcoming);
    EXPECT_LT(played.lightnessF(), upcoming.lightnessF());
    EXPECT_GT(playing.lightnessF(), upcoming.lightnessF());
    EXPECT_NEAR(base.hslHueF(), played.hslHueF(), 0.01);
    EXPECT_NEAR(base.hslHueF(), playing.hslHueF(), 0.01);
    EXPECT_GE(played.lightnessF(), 0.16 - 0.001);
    EXPECT_LE(playing.lightnessF(), 0.72 + 0.001);
}

TEST_F(TandaColorPaletteTest, AutoTextSelectsTheHigherContrastPolarity) {
    EXPECT_EQ(QColor(0xf0, 0xf0, 0xf0),
            TandaColorPalette::autoTextColor(QColor(Qt::black)));
    EXPECT_EQ(QColor(0x10, 0x10, 0x10),
            TandaColorPalette::autoTextColor(QColor(Qt::white)));
    EXPECT_EQ(QColor(0x10, 0x10, 0x10),
            TandaColorPalette::autoTextColor(QColor(QStringLiteral("#c08a2e"))));
}

TEST_F(TandaColorPaletteTest, CursorStateMatchesTrackAndHeaderProgress) {
    EXPECT_EQ(TandaColorState::Upcoming, tandaTrackColorState(2, 0));
    EXPECT_EQ(TandaColorState::Played, tandaTrackColorState(2, 3));
    EXPECT_EQ(TandaColorState::Playing, tandaTrackColorState(3, 3));
    EXPECT_EQ(TandaColorState::Upcoming, tandaTrackColorState(4, 3));

    EXPECT_EQ(TandaColorState::Upcoming, tandaHeaderColorState(3, 4, 0));
    EXPECT_EQ(TandaColorState::Upcoming, tandaHeaderColorState(3, 4, 2));
    EXPECT_EQ(TandaColorState::Playing, tandaHeaderColorState(3, 4, 3));
    EXPECT_EQ(TandaColorState::Playing, tandaHeaderColorState(3, 4, 6));
    EXPECT_EQ(TandaColorState::Played, tandaHeaderColorState(3, 4, 7));
}

} // namespace
