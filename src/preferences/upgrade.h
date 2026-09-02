#pragma once

#include "preferences/usersettings.h"

class Upgrade {
  public:
    enum class ConfigCompatibility {
        Supported,
        NewerThanSupported,
        MigrationUnavailable,
    };

    static constexpr int kInitialTangoQConfigVersion = 1;
    static constexpr int kCurrentTangoQConfigVersion = 1;

    Upgrade();
    ~Upgrade();

    UserSettingsPointer versionUpgrade(const QString& settingsPath);
    bool isFirstRun() const {
        return m_bFirstRun;
    }
    bool rescanLibrary() const {
        return m_bRescanLibrary;
    }
    ConfigCompatibility configCompatibility() const {
        return m_configCompatibility;
    }
    int detectedConfigVersion() const {
        return m_detectedConfigVersion;
    }

  private:
    bool m_bFirstRun;
    bool m_bRescanLibrary;
    ConfigCompatibility m_configCompatibility;
    int m_detectedConfigVersion;
};
