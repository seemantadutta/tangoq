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

    EXPECT_TRUE(palette.colorCodingEnabled());
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

TEST_F(TandaColorPaletteTest, ColorCodingTogglePersistsWithoutChangingColors) {
    TandaColorPalette palette(config());
    QSignalSpy changedSpy(&palette, &TandaColorPalette::changed);
    const QColor custom(QStringLiteral("#123456"));
    palette.setBase(TandaColorCategory::Vals, custom);

    palette.setColorCodingEnabled(false);
    EXPECT_FALSE(palette.colorCodingEnabled());
    EXPECT_EQ(custom, palette.base(TandaColorCategory::Vals));
    EXPECT_EQ(2, changedSpy.count());

    // Writing the same state is a no-op.
    palette.setColorCodingEnabled(false);
    EXPECT_EQ(2, changedSpy.count());

    saveAndReloadConfig();
    const TandaColorPalette restored(config());
    EXPECT_FALSE(restored.colorCodingEnabled());
    EXPECT_EQ(custom, restored.base(TandaColorCategory::Vals));
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

TEST_F(TandaColorPaletteTest, AutoTextSelectsTheHigherContrastPolarity) {
    EXPECT_EQ(QColor(0xf0, 0xf0, 0xf0),
            TandaColorPalette::autoTextColor(QColor(Qt::black)));
    EXPECT_EQ(QColor(0x10, 0x10, 0x10),
            TandaColorPalette::autoTextColor(QColor(Qt::white)));
    EXPECT_EQ(QColor(0x10, 0x10, 0x10),
            TandaColorPalette::autoTextColor(QColor(QStringLiteral("#c08a2e"))));
}

} // namespace
