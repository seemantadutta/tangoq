// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/tandaqueuechecks.h"

#include <gtest/gtest.h>

using tandaqueuechecks::Problem;
using tandaqueuechecks::Row;

namespace {

Row track(const QUuid& tandaId = QUuid()) {
    Row row;
    row.tandaId = tandaId;
    return row;
}

Row cortina(const QString& genre = QString(), const QUuid& tandaId = QUuid()) {
    Row row;
    row.cortina = true;
    row.genre = genre;
    row.tandaId = tandaId;
    return row;
}

} // namespace

TEST(TandaQueueChecksTest, AWellFormedSetHasNoProblems) {
    const QUuid first = QUuid::createUuid();
    const QUuid second = QUuid::createUuid();
    const auto result = tandaqueuechecks::check({
            track(first),
            track(first),
            cortina(QStringLiteral("Pop")),
            track(second),
            track(second),
            cortina(),
            track(), // a loose track, e.g. a performance
    });
    EXPECT_TRUE(result.isEmpty());
}

TEST(TandaQueueChecksTest, ACortinaInsideATandaIsFlaggedOnItsRowAndTanda) {
    const QUuid tanda = QUuid::createUuid();
    const auto result = tandaqueuechecks::check({
            track(tanda),
            cortina(QString(), tanda),
            track(tanda),
    });
    EXPECT_EQ(QList<Problem>{Problem::CortinaInsideTanda}, result.rows.value(1));
    // The header shows it too, so a collapsed tanda still says so.
    EXPECT_EQ(QList<Problem>{Problem::CortinaInsideTanda}, result.tandas.value(tanda));
    EXPECT_FALSE(result.rows.contains(0));
}

TEST(TandaQueueChecksTest, ACortinaWithATandaGenreIsFlagged) {
    const auto result = tandaqueuechecks::check({
            cortina(QStringLiteral("Tango")),
            track(),
            cortina(QStringLiteral("Vals criollo")),
            track(),
            cortina(QStringLiteral("milonga")),
            track(),
            cortina(QStringLiteral("Soundtrack")),
    });
    EXPECT_EQ(QList<Problem>{Problem::CortinaWithTandaGenre}, result.rows.value(0));
    EXPECT_EQ(QList<Problem>{Problem::CortinaWithTandaGenre}, result.rows.value(2));
    EXPECT_EQ(QList<Problem>{Problem::CortinaWithTandaGenre}, result.rows.value(4));
    EXPECT_FALSE(result.rows.contains(6));
}

TEST(TandaQueueChecksTest, OnlyWholeWordsCountAsATandaGenre) {
    EXPECT_TRUE(tandaqueuechecks::isTandaGenre(QStringLiteral("TANGO")));
    EXPECT_TRUE(tandaqueuechecks::isTandaGenre(QStringLiteral("Tango Nuevo")));
    EXPECT_TRUE(tandaqueuechecks::isTandaGenre(QStringLiteral("Vals")));
    EXPECT_FALSE(tandaqueuechecks::isTandaGenre(QStringLiteral("Tangotronic")));
    EXPECT_FALSE(tandaqueuechecks::isTandaGenre(QStringLiteral("Valsa")));
    EXPECT_FALSE(tandaqueuechecks::isTandaGenre(QString()));
}

TEST(TandaQueueChecksTest, TwoCortinasInARowFlagTheSecond) {
    const auto result = tandaqueuechecks::check({
            track(),
            cortina(),
            cortina(),
            track(),
    });
    EXPECT_FALSE(result.rows.contains(1));
    EXPECT_EQ(QList<Problem>{Problem::CortinaAfterCortina}, result.rows.value(2));
}

TEST(TandaQueueChecksTest, TwoTandasWithNoCortinaBetweenFlagTheSecond) {
    const QUuid first = QUuid::createUuid();
    const QUuid second = QUuid::createUuid();
    const QUuid third = QUuid::createUuid();
    const auto result = tandaqueuechecks::check({
            track(first),
            track(first),
            track(second),
            track(second),
            track(), // a loose track between is not a tanda directly after one
            track(third),
    });
    EXPECT_FALSE(result.tandas.contains(first));
    EXPECT_EQ(QList<Problem>{Problem::NoCortinaBeforeTanda}, result.tandas.value(second));
    EXPECT_FALSE(result.tandas.contains(third));
    EXPECT_TRUE(result.rows.isEmpty());
}

TEST(TandaQueueChecksTest, ARowCanHaveSeveralProblems) {
    const auto result = tandaqueuechecks::check({
            cortina(),
            cortina(QStringLiteral("Tango")),
    });
    EXPECT_EQ((QList<Problem>{Problem::CortinaWithTandaGenre, Problem::CortinaAfterCortina}),
            result.rows.value(1));
}
