#include "preferences/upgrade.h"

#include <QFile>
#include <QMap>

#include "config.h"
#include "preferences/settingsmanager.h"
#include "test/mixxxtest.h"
#include "util/versionstore.h"

namespace {

static_assert(Upgrade::kInitialTangoQConfigVersion == 1);

const ConfigKey kProductVersionKey("[Config]", "Version");
const ConfigKey kTangoQConfigVersionKey("[Config]", "TangoQConfigVersion");
const ConfigKey kWaveformTypeKey("[Waveform]", "WaveformType");
const ConfigKey kWaveformFrameRateKey("[Waveform]", "FrameRate");
const ConfigKey kVSyncKey("[Waveform]", "VSync");
const ConfigKey kSentinelKey("[Test]", "Sentinel");
const ConfigKey kFirstRunSkinKey("[Skin]", "show_4decks");
const ConfigKey kFirstRunAutoDjKey("[Auto DJ]", "TransitionMode");
const ConfigKey kUnknownStringKey("[Future Test Group]", "UnknownString");
const ConfigKey kUnknownNumberKey("[Future Test Group]", "UnknownNumber");

using SettingsSnapshot = QMap<ConfigKey, QString>;

SettingsSnapshot snapshotSettings(const UserSettingsPointer& config) {
    SettingsSnapshot snapshot;
    for (const QString& group : config->getGroups()) {
        for (const ConfigKey& key : config->getKeysWithGroup(group)) {
            snapshot.insert(key, config->getValueString(key));
        }
    }
    return snapshot;
}

QByteArray readConfigFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

class UpgradeTest : public MixxxTest {
  protected:
    QString configFilePath() const {
        return getTestDataDir().filePath(MIXXX_SETTINGS_FILE);
    }

    UserSettingsPointer writeExistingConfig(const QString& productVersion) {
        UserSettingsPointer config(new UserSettings(configFilePath()));
        config->setValue(kProductVersionKey, productVersion);
        populateExistingConfig(config);
        EXPECT_TRUE(config->save());
        return config;
    }

    UserSettingsPointer writeExistingConfigWithoutProductVersion() {
        UserSettingsPointer config(new UserSettings(configFilePath()));
        populateExistingConfig(config);
        EXPECT_TRUE(config->save());
        return config;
    }

    void populateExistingConfig(const UserSettingsPointer& config) {
        config->setValue(kWaveformTypeKey, 3);
        config->setValue(kWaveformFrameRateKey, 47);
        config->setValue(kVSyncKey, 0);
        config->setValue(kSentinelKey, QStringLiteral("preserve-me"));
        config->setValue(kUnknownStringKey, QStringLiteral("unknown-value"));
        config->setValue(kUnknownNumberKey, 12345);
    }

    SettingsSnapshot expectedAdoptedSnapshot(const UserSettingsPointer& original) {
        auto expected = snapshotSettings(original);
        expected.insert(kProductVersionKey, VersionStore::forkVersion());
        expected.insert(kTangoQConfigVersionKey,
                QString::number(Upgrade::kCurrentTangoQConfigVersion));
        expected.insert(kVSyncKey, QStringLiteral("0"));
        return expected;
    }

    void expectExistingSettingsPreserved(const UserSettingsPointer& config,
            const SettingsSnapshot& expected) {
        EXPECT_EQ(expected, snapshotSettings(config));
        EXPECT_FALSE(config->exists(kFirstRunSkinKey));
        EXPECT_FALSE(config->exists(kFirstRunAutoDjKey));
    }
};

TEST_F(UpgradeTest, ExistingTangoQConfigIsAdoptedWithoutLegacyMigrations) {
    const auto original = writeExistingConfig(QStringLiteral("1.0.1"));
    const auto expected = expectedAdoptedSnapshot(original);

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_FALSE(upgrade.isFirstRun());
    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config, expected);
}

TEST_F(UpgradeTest, ExistingTangoQDefaultsAreAdoptedWithoutFirstRunChanges) {
    UserSettingsPointer original(new UserSettings(configFilePath()));
    original->setValue(kProductVersionKey, QStringLiteral("1.0.1"));
    original->setValue(kSentinelKey, QStringLiteral("preserve-me"));
    ASSERT_TRUE(original->save());
    const auto expected = expectedAdoptedSnapshot(original);

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_FALSE(upgrade.isFirstRun());
    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_FALSE(config->exists(kWaveformTypeKey));
    EXPECT_FALSE(config->exists(kWaveformFrameRateKey));
    EXPECT_FALSE(config->exists(kFirstRunSkinKey));
    EXPECT_FALSE(config->exists(kFirstRunAutoDjKey));
    EXPECT_QSTRING_EQ("preserve-me", config->getValueString(kSentinelKey));
    EXPECT_EQ(expected, snapshotSettings(config));
}

TEST_F(UpgradeTest, ExistingConfigWithoutProductVersionIsAdopted) {
    const auto original = writeExistingConfigWithoutProductVersion();
    const auto expected = expectedAdoptedSnapshot(original);

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_FALSE(upgrade.isFirstRun());
    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config, expected);
}

class UpgradeProductVersionTest
        : public UpgradeTest,
          public testing::WithParamInterface<const char*> {
};

TEST_P(UpgradeProductVersionTest, MixxxLookingVersionIsOnlyProvenance) {
    const auto original = writeExistingConfig(QString::fromLatin1(GetParam()));
    const auto expected = expectedAdoptedSnapshot(original);

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config, expected);
}

INSTANTIATE_TEST_SUITE_P(MixxxVersions,
        UpgradeProductVersionTest,
        testing::Values("2.0.0", "2.5.6", "3.0.0"));

TEST_F(UpgradeTest, CurrentProductVersionStillAdoptsMissingSchema) {
    const auto original = writeExistingConfig(VersionStore::forkVersion());
    const auto expected = expectedAdoptedSnapshot(original);

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    expectExistingSettingsPreserved(config, expected);
}

TEST_F(UpgradeTest, MissingConfigFileGetsFirstRunDefaults) {
    ASSERT_FALSE(QFile::exists(configFilePath()));

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_TRUE(upgrade.isFirstRun());
    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
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
    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_EQ(4, config->getValue<int>(kFirstRunAutoDjKey));
}

TEST_F(UpgradeTest, ExistingSchemaOneOnlyUpdatesProductVersion) {
    auto original = writeExistingConfig(QStringLiteral("0.9.9"));
    original->setValue(
            kTangoQConfigVersionKey, Upgrade::kCurrentTangoQConfigVersion);
    ASSERT_TRUE(original->save());
    const auto expected = expectedAdoptedSnapshot(original);

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config, expected);
}

TEST_F(UpgradeTest, SchemaZeroIsSafelyAdoptedAsSchemaOne) {
    auto original = writeExistingConfig(QStringLiteral("1.0.1"));
    original->setValue(kTangoQConfigVersionKey, 0);
    ASSERT_TRUE(original->save());
    const auto expected = expectedAdoptedSnapshot(original);

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config, expected);
}

TEST_F(UpgradeTest, MalformedSchemaIsSafelyAdoptedAsSchemaOne) {
    auto original = writeExistingConfig(QStringLiteral("1.0.1"));
    original->setValue(kTangoQConfigVersionKey, QStringLiteral("not-a-number"));
    ASSERT_TRUE(original->save());
    const auto expected = expectedAdoptedSnapshot(original);

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config, expected);
}

TEST_F(UpgradeTest, NegativeSchemaIsSafelyAdoptedAsSchemaOne) {
    auto original = writeExistingConfig(QStringLiteral("1.0.1"));
    original->setValue(kTangoQConfigVersionKey, -7);
    ASSERT_TRUE(original->save());
    const auto expected = expectedAdoptedSnapshot(original);

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_EQ(Upgrade::ConfigCompatibility::Supported,
            upgrade.configCompatibility());
    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            upgrade.detectedConfigVersion());
    expectExistingSettingsPreserved(config, expected);
}

TEST_F(UpgradeTest, FutureSchemaIsLeftUntouched) {
    constexpr int kFutureSchema = Upgrade::kCurrentTangoQConfigVersion + 1;
    auto original = writeExistingConfig(QStringLiteral("future-product"));
    original->setValue(kTangoQConfigVersionKey, kFutureSchema);
    original->setValue(kVSyncKey, 2);
    ASSERT_TRUE(original->save());
    const auto expected = snapshotSettings(original);

    Upgrade upgrade;
    const auto config = upgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_FALSE(upgrade.isFirstRun());
    EXPECT_EQ(Upgrade::ConfigCompatibility::NewerThanSupported,
            upgrade.configCompatibility());
    EXPECT_EQ(kFutureSchema, upgrade.detectedConfigVersion());
    EXPECT_EQ(kFutureSchema, config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ("future-product", config->getValueString(kProductVersionKey));
    EXPECT_EQ(2, config->getValue<int>(kVSyncKey));
    expectExistingSettingsPreserved(config, expected);
}

TEST_F(UpgradeTest, SettingsManagerNeverSavesFutureSchemaConfiguration) {
    constexpr int kFutureSchema = Upgrade::kCurrentTangoQConfigVersion + 1;
    auto original = writeExistingConfig(QStringLiteral("future-product"));
    original->setValue(kTangoQConfigVersionKey, kFutureSchema);
    ASSERT_TRUE(original->save());
    original.reset();
    const QByteArray before = readConfigFile(configFilePath());
    ASSERT_FALSE(before.isEmpty());

    {
        SettingsManager settingsManager(getTestDataDir().path());
        EXPECT_FALSE(settingsManager.isConfigCompatible());
        EXPECT_EQ(kFutureSchema, settingsManager.detectedConfigVersion());
        settingsManager.settings()->setValue(
                kSentinelKey, QStringLiteral("must-not-reach-disk"));
        settingsManager.save();
    }

    EXPECT_EQ(before, readConfigFile(configFilePath()));
}

TEST_F(UpgradeTest, SettingsManagerStillSavesSupportedConfiguration) {
    auto original = writeExistingConfig(QStringLiteral("1.0.1"));
    original->setValue(
            kTangoQConfigVersionKey, Upgrade::kCurrentTangoQConfigVersion);
    ASSERT_TRUE(original->save());
    original.reset();

    {
        SettingsManager settingsManager(getTestDataDir().path());
        ASSERT_TRUE(settingsManager.isConfigCompatible());
        settingsManager.settings()->setValue(
                kSentinelKey, QStringLiteral("saved-value"));
        settingsManager.save();
    }

    UserSettings reloaded(configFilePath());
    EXPECT_QSTRING_EQ("saved-value", reloaded.getValueString(kSentinelKey));
}

TEST_F(UpgradeTest, UpgradeIsIdempotentAfterSaveAndReload) {
    writeExistingConfig(QStringLiteral("1.0.1"));

    Upgrade firstUpgrade;
    auto config = firstUpgrade.versionUpgrade(getTestDataDir().path());
    ASSERT_TRUE(config->save());
    const auto expected = snapshotSettings(config);
    config.reset();

    Upgrade secondUpgrade;
    config = secondUpgrade.versionUpgrade(getTestDataDir().path());

    EXPECT_FALSE(secondUpgrade.isFirstRun());
    EXPECT_EQ(Upgrade::kCurrentTangoQConfigVersion,
            config->getValue<int>(kTangoQConfigVersionKey));
    EXPECT_QSTRING_EQ(VersionStore::forkVersion(),
            config->getValueString(kProductVersionKey));
    expectExistingSettingsPreserved(config, expected);
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
