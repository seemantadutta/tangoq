#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <memory>

#include "preferences/constants.h"
#include "soundio/sounddevicestatus.h"
#include "track/track_decl.h"
#include "util/parented_ptr.h"

class ControlObject;
class ControlProxy;
class QGraphicsOpacityEffect;
class DlgDeveloperTools;
class DlgPreferences;
class DlgKeywheel;
class GuiTick;
class LaunchImage;
class VisualsManager;
class WMainMenuBar;

namespace mixxx {

class CoreServices;

namespace skin {
class SkinLoader;
}

#ifdef __ENGINEPRIME__
class LibraryExporter;
#endif

} // namespace mixxx

/// This Class is the base class for Mixxx.
/// It sets up the main window providing a menubar.
/// For the main view, an instance of class MixxxView is
/// created which creates your view.
class MixxxMainWindow : public QMainWindow {
    Q_OBJECT
  public:
    MixxxMainWindow(std::shared_ptr<mixxx::CoreServices> pCoreServices);
    ~MixxxMainWindow() override;

#ifdef MIXXX_USE_QOPENGL
    void initializeQOpenGL();
#endif
    /// Initialize main window after creation. Should only be called once.
    void initialize();
    /// True if the user tried to close the window during startup (before the
    /// event loop runs). main() checks this after initialize() to quit cleanly
    /// instead of showing the main window.
    bool closeRequestedDuringStartup() const {
        return m_closeRequestedDuringStartup;
    }
    /// creates the menu_bar and inserts the file Menu
    void createMenuBar();
    void connectMenuBar();
    void setInhibitScreensaver(mixxx::preferences::ScreenSaver inhibit);
    mixxx::preferences::ScreenSaver getInhibitScreensaver();

    inline GuiTick* getGuiTick() { return m_pGuiTick; };

  public slots:
    void rebootMixxxView();

    void slotFileLoadSongPlayer(int deck);
    /// show the preferences dialog
    void slotOptionsPreferences();
    /// show the about dialog
    void slotHelpAbout();
    // show keywheel
    void slotShowKeywheel(bool toggle);
    /// toggle full screen mode
    void slotViewFullScreen(bool toggle);
    /// open the developer tools dialog.
    void slotDeveloperTools(bool enable);
    void slotDeveloperToolsClosed();

    void slotUpdateWindowTitle(TrackPointer pTrack);
    /// Reflect Tango DJ mode in the title bar and toolbar logo.
    void slotTangoModeChanged(double value);

    /// warn the user when inputs are not configured.
    void slotNoMicrophoneInputConfigured();
    void slotNoAuxiliaryInputConfigured();
    void slotNoDeckPassthroughInputConfigured();
    void slotNoVinylControlInputConfigured();
#ifndef __APPLE__
    /// Update whether the menubar is toggled pressing the Alt key and show/hide
    /// it accordingly
    void slotUpdateMenuBarAltKeyConnection();
#endif

    void initializationProgressUpdate(int progress, const QString& serviceName);

  private slots:
    void slotTooltipModeChanged(mixxx::preferences::Tooltips tt);

  signals:
    void skinLoaded();
    /// used to uncheck the menu when the dialog of developer tools is closed
    void developerToolsDlgClosed(int r);
    void closeDeveloperToolsDlgChecked(int r);
    void fullScreenChanged(bool fullscreen);

  protected:
    /// Event filter to block certain events (eg. tooltips if tooltips are disabled)
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent* event) override;

  private:
    void initializeWindow();
    /// Centre the window on the screen it is on. Only meaningful once the skin
    /// has sized the window, so it is called after the skin is loaded.
    void centreOnScreen();
    void checkDirectRendering();

    /// Load skin to a QWidget that we set as the central widget.
    bool loadConfiguredSkin();
    void tryParseAndSetDefaultStyleSheet();

    bool confirmExit();
#ifndef __APPLE__
    void alwaysHideMenuBarDlg();
#endif

    QDialog::DialogCode soundDeviceErrorDlg(
            const QString &title, const QString &text, bool* retryClicked);
    QDialog::DialogCode soundDeviceBusyDlg(bool* retryClicked);
    QDialog::DialogCode soundDeviceErrorMsgDlg(
            SoundDeviceStatus status, bool* retryClicked);
    QDialog::DialogCode noOutputDlg(bool* continueClicked);

    std::shared_ptr<mixxx::CoreServices> m_pCoreServices;

    QWidget* m_pCentralWidget;
    LaunchImage* m_pLaunchImage;
#ifndef __APPLE__
    Qt::WindowStates m_prevState;
#endif

    parented_ptr<QMessageBox> m_noVinylInputDialog;
    parented_ptr<QMessageBox> m_noPassthroughInputDialog;
    parented_ptr<QMessageBox> m_noMicInputDialog;
    parented_ptr<QMessageBox> m_noAuxInputDialog;

    std::shared_ptr<mixxx::skin::SkinLoader> m_pSkinLoader;
    GuiTick* m_pGuiTick;
    VisualsManager* m_pVisualsManager;

    parented_ptr<WMainMenuBar> m_pMenuBar;
#ifdef __LINUX__
    const bool m_supportsGlobalMenuBar;
#endif
    bool m_inRebootMixxxView;
    /// False until initialize() has fully run. Startup pumps the event loop
    /// (QApplication::processEvents) to draw launch-image progress before the
    /// real event loop starts, so a close event can arrive while the services
    /// confirmExit() inspects (PlayerManager and friends) do not yet exist.
    /// While this is false, closeEvent() must not touch them.
    bool m_initializationComplete;
    /// Set when a close is requested during startup, so it can be honoured once
    /// initialization has finished. See closeRequestedDuringStartup().
    bool m_closeRequestedDuringStartup;
    /// Whether saved window geometry was restored at startup. False on a first
    /// run, which is when the window gets centred instead.
    bool m_geometryRestored;
    /// Guards centreOnScreen() so it runs only on the first show.
    bool m_geometryCentred;
    /// Tango DJ mode state, shown in the window title and by dimming the logo.
    ControlProxy* m_pTangoModeControl;
    /// The track the title currently shows, so the title can be rebuilt when
    /// only the mode changed.
    TrackPointer m_pTitleTrack;
    QPointer<QGraphicsOpacityEffect> m_pLogoDim;

    DlgDeveloperTools* m_pDeveloperToolsDlg;

    DlgPreferences* m_pPrefDlg;
    parented_ptr<DlgKeywheel> m_pKeywheel;

#ifdef __ENGINEPRIME__
    // Library exporter
    std::unique_ptr<mixxx::LibraryExporter> m_pLibraryExporter;
#endif

    mixxx::preferences::Tooltips m_toolTipsCfg;

    mixxx::preferences::ScreenSaver m_inhibitScreensaver;

    QSet<ControlObject*> m_skinCreatedControls;
};
