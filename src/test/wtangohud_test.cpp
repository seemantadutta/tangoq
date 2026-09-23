// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "widget/wtangohud.h"

#include <gtest/gtest.h>

#include <QColor>
#include <QFile>

#include "test/mixxxtest.h"

namespace {

class WTangoHudTest : public MixxxTest {
};

TEST_F(WTangoHudTest, HighContrastStyleSheetSetsTextColors) {
    // Color schemes recolor the HUD with qproperty-* rules. Qt drops a rule
    // with a misspelled property silently, so apply the real High Contrast
    // rule and check that every color reaches the widget.
    QFile file(getOrInitTestDir().filePath(
            QStringLiteral("../../res/skins/TangoQ/style_classic.qss")));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString styleSheet = QString::fromUtf8(file.readAll());
    const int start = styleSheet.indexOf(QStringLiteral("#TangoHud {"));
    ASSERT_GE(start, 0);
    const int end = styleSheet.indexOf(QChar('}'), start);
    ASSERT_GT(end, start);

    WTangoHud hud;
    hud.setObjectName(QStringLiteral("TangoHud"));
    const QColor defaultText = hud.property("textColor").value<QColor>();
    hud.setStyleSheet(styleSheet.mid(start, end - start + 1));
    hud.ensurePolished();

    EXPECT_NE(QColor(QStringLiteral("#111111")), defaultText);
    EXPECT_EQ(QColor(QStringLiteral("#111111")),
            hud.property("textColor").value<QColor>());
    EXPECT_EQ(QColor(QStringLiteral("#4d4d40")),
            hud.property("columnLabelColor").value<QColor>());
    EXPECT_EQ(QColor(QStringLiteral("#1a1a1a")),
            hud.property("columnValueColor").value<QColor>());
    EXPECT_EQ(QColor(QStringLiteral("#a01010")),
            hud.property("overColor").value<QColor>());
    EXPECT_EQ(QColor(QStringLiteral("#155a24")),
            hud.property("underColor").value<QColor>());
}

} // namespace
