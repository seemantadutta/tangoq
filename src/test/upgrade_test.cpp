#include "test/mixxxtest.h"

#include <QFile>

#include "config.h"
#include "preferences/upgrade.h"
#include "util/versionstore.h"

namespace {

constexpr int kCurrentTangoQConfigVersion = 1;

const ConfigKey kProductVersionKey("[Config]", "Version");
const ConfigKey kTangoQConfigVersionKey("[Config]", "TangoQConfigVersion");
const ConfigKey kWaveformTypeKey("[Waveform]", "WaveformType");
const ConfigKey kWaveformFrameRateKey("[Waveform]", "FrameRate");
const ConfigKey kVSyncKey("[Waveform]", "VSync");
const ConfigKey kSentinelKey("[Test]", "Sentinel");
const ConfigKey kFirstRunSkinKey("[Skin]", "show_4decks");
const ConfigKey kFirstRunAutoDjKey("[Auto DJ]", "TransitionMode");

class UpgradeTest : public MixxxTest {
  protected:
    QString configFilePath() const {
        return getTestDataDir().filePath(MIXXX_SETTINGS_FILE);
    }

    UserSettingsPointer writeExistingConfig(const QString& productVersion) {
        UserSettingsPointer config(new UserSettings(configFilePath()));
        if (!productVersion.isNull()) {
            config->setValue(kProductVersionKey, productVersion);
        }
        config->setValue(kWaveformTypeKey, 3);
        config->setValue(kWaveformFrameRateKey, 47);
        config->setValue(kVSyncKey, 0);
        config->setValue(kSentinelKey, QStringLiteral("preserve-me"));
        EXPECT_TRUE(config->save());
        return config;
    }

    void expectExistingSettingsPreserved(const UserSettingsPointer& config) {
        EXPECT_EQ(3, config->getValue<int>(kWaveformTypeKey));
        EXPECT_EQ(47, config->getValue<int>(kWaveformFrameRateKey));
        EXPECT_QSTRING_EQ("preserve-me", config->getValueString(kSentinelKey));
        EXPECT_FALSE(config->exists(kFirstRunSkinKey));
        EXPECT_FALSE(config->exists(kFirstRunAutoDjKey));
    }
};

TEST_F(UpgradeTest, ExistingTangoQConfigIsAdoptedWithoutLegacyMigrations) {
    writeExistingConfig(QStringLiteral("1.0.1"));

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_FALSE(upgrade.isFirstRun());
    EXPECT_EQ(kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config);
}

TEST_F(UpgradeTest, ExistingTangoQDefaultsAreAdoptedWithoutFirstRunChanges) {
    UserSettingsPointer original(new UserSettings(configFilePath()));
    original->setValue(kProductVersionKey, QStringLiteral("1.0.1"));
    original->setValue(kSentinelKey, QStringLiteral("preserve-me"));
    ASSERT_TRUE(original->save());

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_FALSE(upgrade.isFirstRun());
    EXPECT_EQ(kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_FALSE(config->exists(kWaveformTypeKey));
    EXPECT_FALSE(config->exists(kWaveformFrameRateKey));
    EXPECT_FALSE(config->exists(kFirstRunSkinKey));
    EXPECT_FALSE(config->exists(kFirstRunAutoDjKey));
    EXPECT_QSTRING_EQ("preserve-me", config->getValueString(kSentinelKey));
}

TEST_F(UpgradeTest, ExistingConfigWithoutProductVersionIsAdopted) {
    writeExistingConfig(QString());

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_FALSE(upgrade.isFirstRun());
    EXPECT_EQ(kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config);
}

class UpgradeProductVersionTest
        : public UpgradeTest,
          public testing::WithParamInterface<const char*> {
};

TEST_P(UpgradeProductVersionTest, MixxxLookingVersionIsOnlyProvenance) {
    writeExistingConfig(QString::fromLatin1(GetParam()));

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config);
}

INSTANTIATE_TEST_SUITE_P(MixxxVersions,
        UpgradeProductVersionTest,
        testing::Values("2.0.0", "2.5.6", "3.0.0"));

TEST_F(UpgradeTest, CurrentProductVersionStillAdoptsMissingSchema) {
    writeExistingConfig(VersionStore::forkVersion());

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    expectExistingSettingsPreserved(config);
}

TEST_F(UpgradeTest, MissingConfigFileGetsFirstRunDefaults) {
    ASSERT_FALSE(QFile::exists(configFilePath()));

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_TRUE(upgrade.isFirstRun());
    EXPECT_EQ(kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    EXPECT_NE(VersionStore::version(), config->getValueString(kProductVersionKey));
    EXPECT_EQ(0, config->getValue<int>(kFirstRunSkinKey));
    EXPECT_EQ(4, config->getValue<int>(kFirstRunAutoDjKey));
}

TEST_F(UpgradeTest, EmptyConfigFileGetsFirstRunDefaults) {
    QFile configFile(configFilePath());
    ASSERT_TRUE(configFile.open(QIODevice::WriteOnly));
    configFile.close();

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_TRUE(upgrade.isFirstRun());
    EXPECT_EQ(kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_EQ(4, config->getValue<int>(kFirstRunAutoDjKey));
}

TEST_F(UpgradeTest, ExistingSchemaOneOnlyUpdatesProductVersion) {
    auto original = writeExistingConfig(QStringLiteral("0.9.9"));
    original->setValue(kTangoQConfigVersionKey, kCurrentTangoQConfigVersion);
    ASSERT_TRUE(original->save());

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config);
}

TEST_F(UpgradeTest, SchemaZeroIsSafelyAdoptedAsSchemaOne) {
    auto original = writeExistingConfig(QStringLiteral("1.0.1"));
    original->setValue(kTangoQConfigVersionKey, 0);
    ASSERT_TRUE(original->save());

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config);
}

TEST_F(UpgradeTest, MalformedSchemaIsSafelyAdoptedAsSchemaOne) {
    auto original = writeExistingConfig(QStringLiteral("1.0.1"));
    original->setValue(kTangoQConfigVersionKey, QStringLiteral("not-a-number"));
    ASSERT_TRUE(original->save());

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config);
}

TEST_F(UpgradeTest, FutureSchemaIsLeftUntouched) {
    auto original = writeExistingConfig(QStringLiteral("future-product"));
    original->setValue(kTangoQConfigVersionKey, 2);
    original->setValue(kVSyncKey, 2);
    ASSERT_TRUE(original->save());

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_FALSE(upgrade.isFirstRun());
    EXPECT_EQ(2, config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ("future-product", config->getValueString(kProductVersionKey));
    EXPECT_EQ(2, config->getValue<int>(kVSyncKey));
    expectExistingSettingsPreserved(config);
}

TEST_F(UpgradeTest, UpgradeIsIdempotentAfterSaveAndReload) {
    writeExistingConfig(QStringLiteral("1.0.1"));

    Upgrade firstUpgrade;
    auto config = firstUpgrade.versionUpgrade(getTestDataDir().path());
    ASSERT_TRUE(config->save());
    config.reset();

    Upgrade secondUpgrade;
    config = secondUpgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_FALSE(secondUpgrade.isFirstRun());
    EXPECT_EQ(kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config);
}

TEST_F(UpgradeTest, ExistingConfigsRetainVSyncCompatibilityNormalization) {
    auto original = writeExistingConfig(QStringLiteral("1.0.1"));
    original->setValue(kVSyncKey, 2);
    ASSERT_TRUE(original->save());

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(0, config->getValue<int>(kVSyncKey));
}

} // namespace
