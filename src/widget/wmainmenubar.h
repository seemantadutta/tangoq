#pragma once

#include <QList>
#include <QMenuBar>
#include <QObject>
#include <QScopedPointer>

#include "control/controlproxy.h"
#include "preferences/configobject.h"
#include "preferences/usersettings.h"

class QAction;

class VisibilityControlConnection : public QObject {
    Q_OBJECT
  public:
    // When gateKey is a valid control, the action is only enabled while that
    // control is non-zero (in addition to the visibility control existing). Used
    // to gate the Auto DJ Queue item behind Tango mode.
    VisibilityControlConnection(QObject* pParent,
            QAction* pAction,
            const ConfigKey& key,
            const ConfigKey& gateKey = ConfigKey());

  public slots:
    void slotClearControl();
    void slotReconnectControl();

  private slots:
    void slotControlChanged();
    void slotGateChanged();
    void slotActionToggled(bool toggle);

  private:
    // Recomputes the action's enabled state from the visibility control's
    // validity, and (when gated) its visibility from the gate control - the
    // action is hidden entirely while the gate is closed.
    void updateActionState();

    ConfigKey m_key;
    ConfigKey m_gateKey;
    QScopedPointer<ControlProxy> m_pControl;
    QScopedPointer<ControlProxy> m_pGateControl;
    QAction* m_pAction;
};

class WMainMenuBar : public QMenuBar {
    Q_OBJECT
  public:
    WMainMenuBar(QWidget* pParent, UserSettingsPointer pConfig, ConfigObject<ConfigValueKbd>* pKbdConfig);
#ifndef __APPLE__
    void hideMenuBar();
    void showMenuBar();
#endif

  public slots:
    void onLibraryScanStarted();
    void onLibraryScanFinished();
    void onRecordingStateChange(bool recording);
    void onBroadcastingStateChange(bool broadcasting);
    void onNewSkinAboutToLoad();
    void onNewSkinLoaded();
    void onDeveloperToolsHidden();
    void onDeveloperToolsShown();
    void onFullScreenStateChange(bool fullscreen);
    void onVinylControlDeckEnabledStateChange(int deck, bool enabled);
    void onNumberOfDecksChanged(int decks);
    void onKeywheelChange(int state);
#ifndef __APPLE__
    void slotToggleMenuBar();
#endif

  signals:
    void createCrate();
    void createPlaylist();
    void loadTrackToDeck(int deck);
    void reloadSkin();
    void rescanLibrary();
#ifdef __ENGINEPRIME__
    void exportLibrary();
#endif
    void menubarAutoHideChanged(bool autohide);
    void showAbout();
    void showKeywheel(bool visible);
    void showPreferences();
    void toggleDeveloperTools(bool toggle);
    void toggleFullScreen(bool toggle);
    void toggleKeyboardShortcuts(bool toggle);
    void toggleBroadcasting(bool toggle);
    void toggleRecording(bool enabled);
    void toggleVinylControl(int deck);
    void visitUrl(const QString& url);
    void quit();

    void internalRecordingStateChange(bool recording);
    void internalBroadcastingStateChange(bool broadcasting);
    void internalFullScreenStateChange(bool fullscreen);
    void internalLibraryScanActive(bool active);
    void internalDeveloperToolsStateChange(bool visible);
    void internalKeywheelStateChanged(int state);
    void internalOnNewSkinLoaded();
    void internalOnNewSkinAboutToLoad();

  private slots:
#ifndef __APPLE__
    void slotAutoHideMenuBarToggled(bool autoHide);
#endif
    void slotDeveloperStatsExperiment(bool enable);
    void slotDeveloperStatsBase(bool enable);
    void slotDeveloperDebugger(bool toggle);
    void slotVisitUrl(const QUrl& url);

  private:
    void initialize();
#ifndef __APPLE__
    /// this ensures the menubar is shown when a menu hotkey is pressed
    /// while the menubar is hidden
    void connectMenuToSlotShowMenuBar(const QMenu* pMenu);
#endif
    void createVisibilityControl(QAction* pAction,
            const ConfigKey& key,
            const ConfigKey& gateKey = ConfigKey());

    UserSettingsPointer m_pConfig;
    QAction* m_pViewKeywheel;
    ConfigObject<ConfigValueKbd>* m_pKbdConfig;
    QList<QAction*> m_loadToDeckActions;
    QList<QAction*> m_vinylControlEnabledActions;
};
