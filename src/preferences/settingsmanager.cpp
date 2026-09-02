#include "preferences/settingsmanager.h"

#include <QDir>

#include "control/control.h"
#include "preferences/upgrade.h"
#include "util/assert.h"

SettingsManager::SettingsManager(const QString& settingsPath)
        : m_bShouldRescanLibrary(false),
          m_configCompatibility(Upgrade::ConfigCompatibility::Supported),
          m_detectedConfigVersion(0) {
    // First make sure the settings path exists. If we don't then other parts of
    // Mixxx (such as the library) will produce confusing errors.
    if (!QDir(settingsPath).exists()) {
        QDir().mkpath(settingsPath);
    }

    // Adopt or migrate the TangoQ configuration before other services consume it.
    Upgrade upgrader;
    m_pSettings = upgrader.versionUpgrade(settingsPath);
    VERIFY_OR_DEBUG_ASSERT(!m_pSettings.isNull()) {
        m_pSettings = UserSettingsPointer(new UserSettings(""));
    }
    m_bShouldRescanLibrary = upgrader.rescanLibrary();
    m_configCompatibility = upgrader.configCompatibility();
    m_detectedConfigVersion = upgrader.detectedConfigVersion();

    ControlDoublePrivate::setUserConfig(m_pSettings);

#ifdef __BROADCAST__
    m_pBroadcastSettings = BroadcastSettingsPointer(
                               new BroadcastSettings(m_pSettings));
#endif
}

SettingsManager::~SettingsManager() {
    ControlDoublePrivate::setUserConfig(UserSettingsPointer());
}

void SettingsManager::save() {
    if (!isConfigCompatible()) {
        qWarning() << "Not saving an unsupported TangoQ configuration at schema"
                   << m_detectedConfigVersion;
        return;
    }
    m_pSettings->save();
}
