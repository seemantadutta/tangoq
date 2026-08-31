#include "skin/legacy/legacyskin.h"

#include <QFile>
#include <QTemporaryDir>

#include "test/mixxxtest.h"

namespace {

constexpr char kSkinXml[] = R"(
<!DOCTYPE skin>
<skin>
  <manifest>
    <description>Manifest description</description>
  </manifest>
  <Schemes>
    <Scheme>
      <Name>Default</Name>
      <Description>Default scheme description</Description>
    </Scheme>
    <Scheme>
      <Name>Without Description</Name>
    </Scheme>
  </Schemes>
</skin>
)";

} // namespace

TEST(LegacySkinTest, UsesSchemeDescriptionWithManifestFallback) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    QFile skinFile(tempDir.filePath(QStringLiteral("skin.xml")));
    ASSERT_TRUE(skinFile.open(QFile::WriteOnly | QFile::Text));
    ASSERT_EQ(skinFile.write(kSkinXml), static_cast<qint64>(sizeof(kSkinXml) - 1));
    skinFile.close();

    const mixxx::skin::legacy::LegacySkin skin(QFileInfo(tempDir.path()));
    EXPECT_QSTRING_EQ(QStringLiteral("Default scheme description"),
            skin.description(QStringLiteral("Default")));
    EXPECT_QSTRING_EQ(QStringLiteral("Manifest description"),
            skin.description(QStringLiteral("Without Description")));
    EXPECT_QSTRING_EQ(QStringLiteral("Manifest description"),
            skin.description(QStringLiteral("Unknown")));
    EXPECT_QSTRING_EQ(QStringLiteral("Manifest description"), skin.description());
}
