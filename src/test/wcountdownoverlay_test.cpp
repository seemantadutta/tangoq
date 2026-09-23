// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "widget/wcountdownoverlay.h"

#include <gtest/gtest.h>

#include <QColor>
#include <QFile>
#include <QImage>

#include "test/mixxxtest.h"

namespace {

class WCountdownOverlayTest : public MixxxTest {
};

TEST_F(WCountdownOverlayTest, StartsWithAFullBarAlongTheBottom) {
    WCountdownOverlay overlay;
    overlay.resize(60, 20);
    overlay.start(3000);
    const QImage image = overlay.grab().toImage();
    const QColor bar = overlay.property("barColor").value<QColor>();
    const QColor background = overlay.property("backgroundColor").value<QColor>();

    // The bar spans the whole bottom edge at the start.
    EXPECT_EQ(bar, image.pixelColor(0, 19));
    EXPECT_EQ(bar, image.pixelColor(59, 19));
    // Above it, away from the label, is the background.
    EXPECT_EQ(background, image.pixelColor(1, 1));
    EXPECT_NE(bar, background);
}

TEST_F(WCountdownOverlayTest, HighContrastStyleSheetSetsTheBarColor) {
    // Qt drops a misspelled qproperty silently, so apply the real rule.
    QFile file(getOrInitTestDir().filePath(
            QStringLiteral("../../res/skins/TangoQ/style_classic.qss")));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString styleSheet = QString::fromUtf8(file.readAll());
    const int start = styleSheet.indexOf(QStringLiteral("WCountdownOverlay {"));
    ASSERT_GE(start, 0);
    const int end = styleSheet.indexOf(QChar('}'), start);
    ASSERT_GT(end, start);

    WCountdownOverlay overlay;
    overlay.setStyleSheet(styleSheet.mid(start, end - start + 1));
    overlay.ensurePolished();
    EXPECT_EQ(QColor(QStringLiteral("#d09300")),
            overlay.property("barColor").value<QColor>());
}

} // namespace
