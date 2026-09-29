// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "widget/wlabel.h"

#include <gtest/gtest.h>

#include "test/mixxxtest.h"

// A deck title shows its tags ([CORTINA], [PAUSE AFTER], ...) in their own
// colour ahead of the title, which keeps the label's normal colour.

class WLabelTest : public MixxxTest {};

TEST_F(WLabelTest, APrefixIsShownInItsOwnColourAheadOfTheText) {
    WLabel label;
    label.setTextWithPrefix(QStringLiteral("[PAUSE AFTER]"),
            QColor(0xee, 0x44, 0x44),
            QStringLiteral("Se Va La Vida"));

    EXPECT_EQ(Qt::RichText, label.textFormat());
    EXPECT_EQ(QStringLiteral(
                      "<span style=\"color:#ee4444\">[PAUSE AFTER]</span>"
                      "&nbsp;Se Va La Vida"),
            label.QLabel::text());
    // Everything that reads the text sees it plain.
    EXPECT_EQ(QStringLiteral("[PAUSE AFTER] Se Va La Vida"), label.text());
}

TEST_F(WLabelTest, ATitleIsNeverTreatedAsMarkup) {
    WLabel label;
    label.setTextWithPrefix(QStringLiteral("[CORTINA]"),
            QColor(0xee, 0x44, 0x44),
            QStringLiteral("<b>Tom & Jerry</b>"));

    EXPECT_TRUE(label.QLabel::text().endsWith(
            QStringLiteral("&lt;b&gt;Tom &amp; Jerry&lt;/b&gt;")));
}

TEST_F(WLabelTest, PlainTextAgainOnceThePrefixIsGone) {
    WLabel label;
    label.setTextWithPrefix(QStringLiteral("[CORTINA]"),
            QColor(0xee, 0x44, 0x44),
            QStringLiteral("Espresso"));
    label.setText(QStringLiteral("Espresso"));

    EXPECT_EQ(Qt::AutoText, label.textFormat());
    EXPECT_EQ(QStringLiteral("Espresso"), label.QLabel::text());
    EXPECT_EQ(QStringLiteral("Espresso"), label.text());
}
