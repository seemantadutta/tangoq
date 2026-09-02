#pragma once

#ifdef __BROADCAST__
#include "preferences/broadcastsettings.h"
#endif
#include "preferences/upgrade.h"
#include "preferences/usersettings.h"

class SettingsManager {
  public:
    explicit SettingsManager(const QString& settingsPath);
    virtual ~SettingsManager();

    UserSettingsPointer settings() const {
        return m_pSettings;
    }

#ifdef __BROADCAST__
    BroadcastSettingsPointer broadcastSettings() const {
        return m_pBroadcastSettings;
    }
#endif

    void save();

    bool isConfigCompatible() const {
        return m_configCompatibility == Upgrade::ConfigCompatibility::Supported;
    }

    Upgrade::ConfigCompatibility configCompatibility() const {
        return m_configCompatibility;
    }

    int detectedConfigVersion() const {
        return m_detectedConfigVersion;
    }

    bool shouldRescanLibrary() const {
        return m_bShouldRescanLibrary;
    }

  private:
    UserSettingsPointer m_pSettings;
    bool m_bShouldRescanLibrary;
    Upgrade::ConfigCompatibility m_configCompatibility;
    int m_detectedConfigVersion;
#ifdef __BROADCAST__
    BroadcastSettingsPointer m_pBroadcastSettings;
#endif
};
