#include "preferences/upgrade.h"

#include <QDir>
#include <QFileInfo>

#include "config.h"
#include "util/versionstore.h"
#include "waveform/vsyncthread.h"

namespace {

constexpr int kCurrentTangoQConfigVersion = 1;

const ConfigKey kProductVersionKey("[Config]", "Version");
const ConfigKey kTangoQConfigVersionKey("[Config]", "TangoQConfigVersion");

// Defaults for a brand new install. TangoQ targets tango DJs, for whom the
// stock club layout -- four decks, samplers, effect racks, spinnies -- is mostly
// noise. Writing these values only for a genuinely empty configuration lets a
// fresh install open ready to use while ensuring every later user change wins.
void applyFirstRunDefaults(const UserSettingsPointer& config) {
    static constexpr std::pair<const char*, const char*> kSkinDefaults[] = {
            // Hidden: club-oriented features a tango DJ does not use.
            {"show_4decks", "0"},
            {"show_4effectunits", "0"},
            {"show_effectrack", "0"},
            {"show_samplers", "0"},
            {"show_sampler_fx", "0"},
            {"show_microphones", "0"},
            {"show_vinylcontrol", "0"},
            {"show_spinnies", "0"},
            {"show_superknobs", "0"},
            {"show_coverart", "0"},
            {"show_library_coverart", "0"},
            {"select_big_spinny_or_cover", "0"},
            {"show_hotcues", "0"},
            {"show_intro_outro_cues", "0"},
            {"show_loop_controls", "0"},
            {"show_beatjump_controls", "0"},
            {"show_key_controls", "0"},
            {"show_rate_controls", "0"},
            {"show_eq_kill_buttons", "0"},
            {"show_main_head_mixer", "0"},
            {"show_maximized_library", "0"},
            {"timing_shift_buttons", "0"},
            // Kept: what is actually needed to cue and mix a tanda.
            {"show_mixer", "1"},
            {"show_xfader", "1"},
            {"show_eq_knobs", "1"},
            {"show_preview_decks", "1"},
            {"show_beatgrid_controls", "1"},
            {"show_rate_control_buttons", "1"},
            {"show_8_hotcues", "1"},
            // Compact variants of the controls that remain.
            {"show_loop_controls_compact", "1"},
            {"show_beatjump_controls_compact", "1"},
            {"show_key_controls_compact", "1"},
            {"show_rate_controls_compact", "1"},
            {"show_sync_button_compact", "1"},
            {"show_vumeters_compact", "1"},
    };
    for (const auto& [key, value] : kSkinDefaults) {
        config->set(ConfigKey("[Skin]", key),
                ConfigValue(QString::fromLatin1(value)));
    }

    // Tandas need silence between trimmed track boundaries. "4" is
    // TransitionMode::TandaTransition (see autodjprocessor.h); TandaGap is
    // positive silence in seconds.
    config->set(ConfigKey("[Auto DJ]", "TransitionMode"), ConfigValue("4"));
    config->set(ConfigKey("[Auto DJ]", "Transition"), ConfigValue("-3"));
    config->set(ConfigKey("[Auto DJ]", "TandaGap"), ConfigValue("3"));

    // Cortinas should fade in and out by default. "1" selects Cortina Fade
    // (0 is hard cut); the fade-in/out values are seconds.
    config->set(ConfigKey("[Auto DJ]", "CortinaFadeMode"), ConfigValue("1"));
    config->set(ConfigKey("[Auto DJ]", "CortinaFadeIn"), ConfigValue("5"));
    config->set(ConfigKey("[Auto DJ]", "CortinaFadeOut"), ConfigValue("5"));
}

VSyncThread::VSyncMode upgradeDeprecatedVSyncModes(int configVSyncMode) {
    using VT = VSyncThread;
    if (configVSyncMode >= 0 && configVSyncMode <= static_cast<int>(VT::ST_COUNT)) {
        switch (static_cast<VSyncThread::VSyncMode>(configVSyncMode)) {
        case VT::ST_DEFAULT:
            return VT::ST_DEFAULT;
        case VT::ST_MESA_VBLANK_MODE_1_DEPRECATED:
            return VT::ST_DEFAULT;
        case VT::ST_SGI_VIDEO_SYNC_DEPRECATED:
            return VT::ST_DEFAULT;
        case VT::ST_OML_SYNC_CONTROL_DEPRECATED:
            return VT::ST_DEFAULT;
        case VT::ST_FREE:
            return VT::ST_FREE;
        case VT::ST_TIMER:
            return VT::ST_TIMER;
        case VT::ST_PLL:
            return VT::ST_PLL;
        case VT::ST_COUNT:
            return VT::ST_DEFAULT;
        }
    }

    return VT::ST_DEFAULT;
}

} // namespace

Upgrade::Upgrade()
        : m_bFirstRun(false),
          m_bRescanLibrary(false) {
}

Upgrade::~Upgrade() {
}

UserSettingsPointer Upgrade::versionUpgrade(const QString& settingsPath) {
    // TangoQ owns tangoq.cfg. In particular, do not inspect or move legacy
    // Mixxx files from the user's home directory into TangoQ's settings path.
    const QString configFilePath =
            QDir(settingsPath).filePath(MIXXX_SETTINGS_FILE);
    const QFileInfo configFileInfo(configFilePath);
    const bool isEmptyConfig =
            !configFileInfo.exists() || configFileInfo.size() == 0;
    UserSettingsPointer config(new UserSettings(configFilePath));

    if (isEmptyConfig) {
        qDebug() << "Initializing a new TangoQ configuration at schema"
                 << kCurrentTangoQConfigVersion;
        applyFirstRunDefaults(config);
        config->setValue(kProductVersionKey, VersionStore::forkVersion());
        config->setValue(
                kTangoQConfigVersionKey, kCurrentTangoQConfigVersion);
        m_bFirstRun = true;
        return config;
    }

    int configSchemaVersion = kCurrentTangoQConfigVersion;
    if (!config->exists(kTangoQConfigVersionKey)) {
        // All existing pre-schema tangoq.cfg files are adopted without
        // transforming their settings, regardless of the product version they
        // carry. The product version is provenance, not a migration selector.
        qDebug() << "Adopting existing TangoQ configuration as schema"
                 << kCurrentTangoQConfigVersion;
        config->setValue(
                kTangoQConfigVersionKey, kCurrentTangoQConfigVersion);
    } else {
        const QString rawSchemaVersion =
                config->getValueString(kTangoQConfigVersionKey);
        bool parsed = false;
        configSchemaVersion = rawSchemaVersion.toInt(&parsed);

        if (!parsed || configSchemaVersion < 0) {
            // Schema 1 has no transformations, so adopting a malformed value
            // is safe. Keep this explicit when future migrations are added.
            qWarning() << "Invalid TangoQ configuration schema"
                       << rawSchemaVersion << "-- safely adopting schema"
                       << kCurrentTangoQConfigVersion;
            configSchemaVersion = kCurrentTangoQConfigVersion;
            config->setValue(
                    kTangoQConfigVersionKey, configSchemaVersion);
        } else if (configSchemaVersion > kCurrentTangoQConfigVersion) {
            // An older binary cannot know whether any setting written by a
            // future schema is safe to rewrite. Leave the configuration
            // entirely untouched, including product provenance and VSync.
            qWarning() << "TangoQ configuration schema" << configSchemaVersion
                       << "is newer than supported schema"
                       << kCurrentTangoQConfigVersion
                       << "-- leaving configuration unchanged";
            return config;
        }
    }

    // Apply each supported TangoQ migration in sequence. Schema 0 represents
    // an existing configuration from before the counter was introduced;
    // moving it to schema 1 is adoption only and transforms no settings.
    while (configSchemaVersion < kCurrentTangoQConfigVersion) {
        switch (configSchemaVersion) {
        case 0:
            qDebug() << "Adopting TangoQ configuration schema 0 as schema 1";
            configSchemaVersion = 1;
            break;
        default:
            qWarning() << "No TangoQ configuration migration from schema"
                       << configSchemaVersion << "to supported schema"
                       << kCurrentTangoQConfigVersion
                       << "-- leaving remaining settings unchanged";
            return config;
        }
        config->setValue(kTangoQConfigVersionKey, configSchemaVersion);
    }

    // Preserve the existing always-on compatibility normalization. This is
    // independent of both product releases and TangoQ schema migrations.
    config->setValue(ConfigKey("[Waveform]", "VSync"),
            upgradeDeprecatedVSyncModes(config->getValue(
                    ConfigKey("[Waveform]", "VSync"), 0)));

    config->setValue(kProductVersionKey, VersionStore::forkVersion());
    qDebug() << "TangoQ configuration is at schema" << configSchemaVersion
             << "for product" << VersionStore::forkVersion();
    return config;
}
