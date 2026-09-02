#include "test/mixxxtest.h"

#ifdef Q_OS_MACOS

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "config.h"
#include "util/sandbox.h"

TEST(SandboxTest, TangoQSettingsPathHasNoLegacyMixxxSideEffects) {
    QTemporaryDir testHome;
    ASSERT_TRUE(testHome.isValid());

    const QString legacyMixxxPath =
            QDir(testHome.path()).filePath(QStringLiteral("Library/Application Support/Mixxx"));
    ASSERT_TRUE(QDir().mkpath(legacyMixxxPath));
    const QString markerPath = QDir(legacyMixxxPath).filePath(QStringLiteral("keep-me"));
    QFile marker(markerPath);
    ASSERT_TRUE(marker.open(QIODevice::WriteOnly));
    ASSERT_EQ(4, marker.write("safe", 4));
    marker.close();

    const QString tangoQPath = Sandbox::settingsPathForHome(testHome.path());
    const QString expectedPath = QDir(testHome.path())
                                         .filePath(QStringLiteral(
                                                 "Library/Containers/%1/Data/Library/"
                                                 "Application Support/%2")
                                                           .arg(QStringLiteral(
                                                                        MACOS_SETTINGS_CONTAINER_ID),
                                                                   QStringLiteral(
                                                                           MACOS_SETTINGS_DIR_NAME)));

    EXPECT_QSTRING_EQ(expectedPath, tangoQPath);
    EXPECT_TRUE(QFileInfo::exists(markerPath));
    EXPECT_TRUE(QDir(legacyMixxxPath).exists());
    EXPECT_FALSE(QFileInfo::exists(tangoQPath));
}

#endif
