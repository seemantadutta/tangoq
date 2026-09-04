#include "widget/wmainmenubar.h"

#ifndef __APPLE__
#include <QApplication>
#include <QWindow>
#endif
#include <QUrl>
#include <algorithm>

#include "config.h"
#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "defs_urls.h"
#include "moc_wmainmenubar.cpp"
#include "util/cmdlineargs.h"
#include "util/desktophelper.h"
#include "util/experiment.h"
#include "util/versionstore.h"
#include "vinylcontrol/defs_vinylcontrol.h"

namespace {

constexpr int kMaxLoadToDeckActions = 4;
const QString kSkinGroup = QStringLiteral("[Skin]");

QString buildWhatsThis(const QString& title, const QString& text) {
    QString preparedTitle = title;
    return QString("%1\n\n%2").arg(preparedTitle.remove("&"), text);
}

// TangoQ: vinylControlDefaultKeyBinding() removed with the Options > Vinyl
// Control menu it fed - an unused static function fails the -Wunused-function
// build. Restore it from git history alongside that menu if vinyl control is
// ever brought back.

QString loadToDeckDefaultKeyBinding(int deck) {
    switch (deck) {
    case 0:
        return QObject::tr("Ctrl+o");
    case 1:
        return QObject::tr("Ctrl+Shift+O");
    default:
        return QString();
    }
}

QString showPreferencesKeyBinding() {
#ifdef __APPLE__
    return QObject::tr("Ctrl+,");
#else
    return QObject::tr("Ctrl+P");
#endif
}

QUrl documentationUrl(
        const QString& resourcePath, const QString& fileName, const QString& docUrl) {
    QDir resourceDir(resourcePath);
    // Documentation PDFs are included on Windows and Linux only,
    // so on macOS this always returns the web URL.
#if defined(MIXXX_INSTALL_DOCDIR_RELATIVE_TO_DATADIR)
    if (!resourceDir.exists(fileName)) {
        resourceDir.cd(MIXXX_INSTALL_DOCDIR_RELATIVE_TO_DATADIR);
    }
#endif
    if (resourceDir.exists(fileName)) {
        return QUrl::fromLocalFile(resourceDir.absoluteFilePath(fileName));
    } else {
        return QUrl(docUrl);
    }
}
} // namespace

WMainMenuBar::WMainMenuBar(QWidget* pParent, UserSettingsPointer pConfig, ConfigObject<ConfigValueKbd>* pKbdConfig)
        : QMenuBar(pParent),
          m_pConfig(pConfig),
          m_pKbdConfig(pKbdConfig) {
    setObjectName(QStringLiteral("MainMenu"));
    initialize();
}

void WMainMenuBar::initialize() {
    // TangoQ: the menu bar must never auto-hide. Losing it mid-gig - and the
    // Alt-to-reveal trap it leaves behind - is unacceptable during a live set, so
    // the setting is forced off on every launch and the View-menu toggle that
    // exposed it is removed below. This also un-hides a bar left hidden by a
    // previous run.
    m_pConfig->setValue(ConfigKey("[Config]", "hide_menubar"), 0);

    // Product name for user-facing menu text, so items read "TangoQ" rather than
    // "Mixxx".
    const QString appName = VersionStore::applicationName();

    // FILE MENU
    QMenu* pFileMenu = new QMenu(tr("&File"), this);
#ifndef __APPLE__
    connectMenuToSlotShowMenuBar(pFileMenu);
#endif

    QString loadTrackText = tr("Load Track to Deck &%1");
    QString loadTrackStatusText = tr("Loads a track in deck %1");
    QString openText = tr("Open");
    for (unsigned int deck = 0; deck < kMaxLoadToDeckActions; ++deck) {
        QString playerLoadStatusText = loadTrackStatusText.arg(QString::number(deck + 1));
        QAction* pFileLoadSongToPlayer = new QAction(
                loadTrackText.arg(QString::number(deck + 1)), this);

        QString binding = m_pKbdConfig->getValue(
                ConfigKey("[KeyboardShortcuts]", QString("FileMenu_LoadDeck%1").arg(deck + 1)),
                loadToDeckDefaultKeyBinding(deck));
        if (!binding.isEmpty()) {
            pFileLoadSongToPlayer->setShortcut(QKeySequence(binding));
            pFileLoadSongToPlayer->setShortcutContext(Qt::ApplicationShortcut);
        }
        pFileLoadSongToPlayer->setStatusTip(playerLoadStatusText);
        pFileLoadSongToPlayer->setWhatsThis(
                buildWhatsThis(openText, playerLoadStatusText));
        // Visibility of load to deck actions is set in
        // WMainMenuBar::onNumberOfDecksChanged.
        pFileLoadSongToPlayer->setVisible(false);
        connect(pFileLoadSongToPlayer, &QAction::triggered, this, [this, deck] { emit loadTrackToDeck(deck + 1); });

        pFileMenu->addAction(pFileLoadSongToPlayer);
        m_loadToDeckActions.push_back(pFileLoadSongToPlayer);
    }

    pFileMenu->addSeparator();

    QString quitTitle = tr("&Exit");
    QString quitText = tr("Quits %1").arg(appName);
    auto* pFileQuit = new QAction(quitTitle, this);
    pFileQuit->setShortcut(
            QKeySequence(m_pKbdConfig->getValue(ConfigKey("[KeyboardShortcuts]", "FileMenu_Quit"),
                    tr("Ctrl+q"))));
    pFileQuit->setShortcutContext(Qt::ApplicationShortcut);
    pFileQuit->setStatusTip(quitText);
    pFileQuit->setWhatsThis(buildWhatsThis(quitTitle, quitText));
    pFileQuit->setMenuRole(QAction::QuitRole);
    connect(pFileQuit, &QAction::triggered, this, &WMainMenuBar::quit);
    pFileMenu->addAction(pFileQuit);

    addMenu(pFileMenu);

    // LIBRARY MENU
    QMenu* pLibraryMenu = new QMenu(tr("&Library"), this);
#ifndef __APPLE__
    connectMenuToSlotShowMenuBar(pLibraryMenu);
#endif

    QString rescanTitle = tr("&Rescan Library");
    QString rescanText = tr("Rescans library folders for changes to tracks.");
    auto* pLibraryRescan = new QAction(rescanTitle, this);
    pLibraryRescan->setShortcut(QKeySequence(m_pKbdConfig->getValue(
            ConfigKey("[KeyboardShortcuts]", "LibraryMenu_Rescan"),
            tr("Ctrl+Shift+L"))));
    pLibraryRescan->setStatusTip(rescanText);
    pLibraryRescan->setWhatsThis(buildWhatsThis(rescanTitle, rescanText));
    pLibraryRescan->setCheckable(false);
    connect(pLibraryRescan, &QAction::triggered, this, &WMainMenuBar::rescanLibrary);
    // Disable the action when a scan is active.
    connect(this, &WMainMenuBar::internalLibraryScanActive, pLibraryRescan, &QAction::setDisabled);
    pLibraryMenu->addAction(pLibraryRescan);
    // TangoQ: a full library rescan is a heavy operation that can stutter audio,
    // so hide "Rescan Library" while a set is running in LIVE mode. Re-evaluated
    // each time the Library menu opens, as WTrackMenu does for its Tango actions.
    connect(pLibraryMenu, &QMenu::aboutToShow, this, [pLibraryRescan] {
        const bool liveMode = ControlObject::get(ConfigKey(
                                      QStringLiteral("[AutoDJ]"),
                                      QStringLiteral("live_mode"))) > 0.0;
        pLibraryRescan->setVisible(!liveMode);
    });

#ifdef __ENGINEPRIME__
    //: "Engine DJ" must not be translated
    QString exportTitle = tr("E&xport Library to Engine DJ");
    QString exportText = tr("Export the library to the Engine DJ format");
    auto* pLibraryExport = new QAction(exportTitle, this);
    pLibraryExport->setStatusTip(exportText);
    pLibraryExport->setWhatsThis(buildWhatsThis(exportTitle, exportText));
    pLibraryExport->setCheckable(false);
    connect(pLibraryExport, &QAction::triggered, this, &WMainMenuBar::exportLibrary);
    pLibraryMenu->addAction(pLibraryExport);
#endif

    pLibraryMenu->addSeparator();

    QString createPlaylistTitle = tr("Create &New Playlist");
    QString createPlaylistText = tr("Create a new playlist");
    auto* pLibraryCreatePlaylist = new QAction(createPlaylistTitle, this);
    pLibraryCreatePlaylist->setShortcut(
            QKeySequence(m_pKbdConfig->getValue(
                    ConfigKey("[KeyboardShortcuts]", "LibraryMenu_NewPlaylist"),
                    tr("Ctrl+n"))));
    pLibraryCreatePlaylist->setShortcutContext(Qt::ApplicationShortcut);
    pLibraryCreatePlaylist->setStatusTip(createPlaylistText);
    pLibraryCreatePlaylist->setWhatsThis(buildWhatsThis(createPlaylistTitle, createPlaylistText));
    connect(pLibraryCreatePlaylist, &QAction::triggered, this, &WMainMenuBar::createPlaylist);
    pLibraryMenu->addAction(pLibraryCreatePlaylist);

    QString createCrateTitle = tr("Create New &Crate");
    QString createCrateText = tr("Create a new crate");
    auto* pLibraryCreateCrate = new QAction(createCrateTitle, this);
    pLibraryCreateCrate->setShortcut(
            QKeySequence(m_pKbdConfig->getValue(ConfigKey("[KeyboardShortcuts]",
                                                        "LibraryMenu_NewCrate"),
                    tr("Ctrl+Shift+N"))));
    pLibraryCreateCrate->setShortcutContext(Qt::ApplicationShortcut);
    pLibraryCreateCrate->setStatusTip(createCrateText);
    pLibraryCreateCrate->setWhatsThis(buildWhatsThis(createCrateTitle, createCrateText));
    connect(pLibraryCreateCrate, &QAction::triggered, this, &WMainMenuBar::createCrate);
    pLibraryMenu->addAction(pLibraryCreateCrate);

    addMenu(pLibraryMenu);

#if defined(__APPLE__)
    // Note: On macOS 10.11 ff. we have to deal with "automagic" menu items,
    // when ever a menu "View" is present. QT (as of 5.12.3) does not handle this for us.
    // Add an invisible suffix to the View item string so it doesn't string-equal "View" ,
    // and the magic menu items won't get injected.
    // https://github.com/mixxxdj/mixxx/issues/8442
    QMenu* pViewMenu = new QMenu(tr("&View") + QStringLiteral("\u200C"), this);
#else
    QMenu* pViewMenu = new QMenu(tr("&View"), this);
    connectMenuToSlotShowMenuBar(pViewMenu);
#endif

    // TangoQ: the "Auto-hide menu bar" toggle is deliberately not offered - see
    // the forced hide_menubar=0 in initialize(). slotAutoHideMenuBarToggled and
    // hideMenuBar() are left in place but are now only reachable through a config
    // value that is always 0.

    // Skin Settings Menu
    QString mayNotBeSupported = tr("May not be supported on all skins.");
    QString showSkinSettingsTitle = tr("Show Skin Settings Menu");
    QString showSkinSettingsText = tr("Show the Skin Settings Menu of the currently selected Skin") +
            " " + mayNotBeSupported;
    auto* pViewShowSkinSettings = new QAction(showSkinSettingsTitle, this);
    pViewShowSkinSettings->setCheckable(true);
    pViewShowSkinSettings->setShortcut(
            QKeySequence(m_pKbdConfig->getValue(
                    ConfigKey("[KeyboardShortcuts]", "ViewMenu_ShowSkinSettings"),
                    tr("Ctrl+1", "Menubar|View|Show Skin Settings"))));
    pViewShowSkinSettings->setStatusTip(showSkinSettingsText);
    pViewShowSkinSettings->setWhatsThis(buildWhatsThis(showSkinSettingsTitle, showSkinSettingsText));
    createVisibilityControl(pViewShowSkinSettings,
            ConfigKey(kSkinGroup, QStringLiteral("show_settings")));
    pViewMenu->addAction(pViewShowSkinSettings);

    // TangoQ: "Show Microphone Section" and "Show Vinyl Control Section" removed -
    // neither belongs in a tanda set, and both sections are force-hidden in the
    // skin.

    QString showPreviewDeckTitle = tr("Show Preview Deck");
    QString showPreviewDeckText = tr("Show the preview deck in the %1 interface.").arg(appName) +
            " " + mayNotBeSupported;
    auto* pViewShowPreviewDeck = new QAction(showPreviewDeckTitle, this);
    pViewShowPreviewDeck->setCheckable(true);
    pViewShowPreviewDeck->setShortcut(
            QKeySequence(m_pKbdConfig->getValue(
                    ConfigKey("[KeyboardShortcuts]", "ViewMenu_ShowPreviewDeck"),
                    tr("Ctrl+4", "Menubar|View|Show Preview Deck"))));
    pViewShowPreviewDeck->setStatusTip(showPreviewDeckText);
    pViewShowPreviewDeck->setWhatsThis(buildWhatsThis(showPreviewDeckTitle, showPreviewDeckText));
    createVisibilityControl(pViewShowPreviewDeck,
            ConfigKey(kSkinGroup, QStringLiteral("show_preview_decks")));
    pViewMenu->addAction(pViewShowPreviewDeck);

    QString showCoverArtTitle = tr("Show Cover Art");
    QString showCoverArtText = tr("Show cover art in the %1 interface.").arg(appName) +
            " " + mayNotBeSupported;
    auto* pViewShowCoverArt = new QAction(showCoverArtTitle, this);
    pViewShowCoverArt->setCheckable(true);
    pViewShowCoverArt->setShortcut(
            QKeySequence(m_pKbdConfig->getValue(
                    ConfigKey("[KeyboardShortcuts]", "ViewMenu_ShowCoverArt"),
                    tr("Ctrl+6", "Menubar|View|Show Cover Art"))));
    pViewShowCoverArt->setStatusTip(showCoverArtText);
    pViewShowCoverArt->setWhatsThis(buildWhatsThis(showCoverArtTitle, showCoverArtText));
    createVisibilityControl(pViewShowCoverArt,
            ConfigKey(kSkinGroup, QStringLiteral("show_library_coverart")));
    pViewMenu->addAction(pViewShowCoverArt);

    // TangoQ: "Show Keywheel" (and its F12 shortcut) removed - harmonic mixing has
    // no place in a tanda set. The QAction is still constructed, but with no
    // shortcut and never added to a menu, so it can never be triggered. It is kept
    // only so onKeywheelChange() (wired to DlgKeywheel::finished) has a valid
    // object; that path is now unreachable but the pointer must not dangle.
    QString keywheelTitle = tr("Show Keywheel");
    m_pViewKeywheel = new QAction(keywheelTitle, this);
    m_pViewKeywheel->setCheckable(true);

    // Dockable, always-visible Auto DJ queue panel, toggled via the
    // [AutoDJ],show_autodj_dock control owned by AutoDJFeature. The control is
    // created later by AutoDJFeature, during CoreServices init (after this menu
    // is first built). Bind through the visibility-control mechanism, which
    // (re)connects the proxy on skin load - by which point the control exists. A
    // direct ControlProxy created here would bind to a throwaway default control
    // and never see the real one.
    QString autoDJQueueTitle = tr("TangoQ Side Panel");
    QString autoDJQueueText =
            tr("Show the TangoQ queue in a dockable side panel that stays "
               "visible while browsing the library. Available in Tango mode "
               "only.");
    auto* pViewAutoDJQueue = new QAction(autoDJQueueTitle, this);
    pViewAutoDJQueue->setCheckable(true);
    pViewAutoDJQueue->setShortcut(
            QKeySequence(m_pKbdConfig->getValue(
                    ConfigKey("[KeyboardShortcuts]", "ViewMenu_ShowAutoDJQueue"),
                    tr("Ctrl+Shift+A", "Menubar|View|Auto DJ Side Panel"))));
    pViewAutoDJQueue->setShortcutContext(Qt::ApplicationShortcut);
    pViewAutoDJQueue->setStatusTip(autoDJQueueText);
    pViewAutoDJQueue->setWhatsThis(buildWhatsThis(autoDJQueueTitle, autoDJQueueText));
    // Gated behind Tango mode ([AutoDJ],keep_queue): the queue panel is only
    // meaningful when the played-track cursor is kept, so the item is hidden
    // entirely outside Tango mode (the menu is unchanged from stock Mixxx then).
    createVisibilityControl(pViewAutoDJQueue,
            ConfigKey(QStringLiteral("[AutoDJ]"), QStringLiteral("show_autodj_dock")),
            ConfigKey(QStringLiteral("[AutoDJ]"), QStringLiteral("keep_queue")));
    pViewMenu->addAction(pViewAutoDJQueue);

    // TangoQ: "Maximize Library" removed. The big-library layout hides the
    // critical Set Start / Reset buttons (it is force-hidden in the skin), and
    // removing the action also removes its Space shortcut - so the space bar no
    // longer maximizes the library by accident during a set.

    pViewMenu->addSeparator();

    QString fullScreenTitle = tr("&Full Screen");
    QString fullScreenText = tr("Display %1 using the full screen").arg(appName);
    auto* pViewFullScreen = new QAction(fullScreenTitle, this);
    QList<QKeySequence> shortcuts;
    // We use F11 _AND_ the OS shortcut only on Linux and Windows because on
    // newer macOS versions there might be issues with getting F11 working.
    // https://github.com/mixxxdj/mixxx/pull/3011#issuecomment-678678328
#ifndef __APPLE__
    shortcuts << QKeySequence("F11");
#endif
    QKeySequence osShortcut = QKeySequence::FullScreen;
    // Note(ronso0) Only add the OS shortcut if it's not empty and not F11.
    // In some Linux distros the window managers doesn't pass the OS fullscreen
    // key sequence to Mixxx for some reason.
    // Both adding an empty key sequence or the same sequence twice can render
    // the fullscreen shortcut nonfunctional.
    // https://github.com/mixxxdj/mixxx/issues/10005  PR #3011
    if (!osShortcut.isEmpty() && !shortcuts.contains(osShortcut)) {
        shortcuts << osShortcut;
    }

    pViewFullScreen->setShortcuts(shortcuts);
    pViewFullScreen->setShortcutContext(Qt::ApplicationShortcut);
    pViewFullScreen->setCheckable(true);
    pViewFullScreen->setChecked(false);
    pViewFullScreen->setStatusTip(fullScreenText);
    pViewFullScreen->setWhatsThis(buildWhatsThis(fullScreenTitle, fullScreenText));
    connect(pViewFullScreen, &QAction::triggered, this, &WMainMenuBar::toggleFullScreen);
    connect(this,
            &WMainMenuBar::internalFullScreenStateChange,
            pViewFullScreen,
            &QAction::setChecked);
    pViewMenu->addAction(pViewFullScreen);

    addMenu(pViewMenu);

    // OPTIONS MENU
    QMenu* pOptionsMenu = new QMenu(tr("&Options"), this);
#ifndef __APPLE__
    connectMenuToSlotShowMenuBar(pOptionsMenu);
#endif

    // TangoQ: "Vinyl Control", "Record Mix" and "Enable Live Broadcasting" are
    // removed from the Options menu - a tanda set is not mixed on timecode vinyl,
    // recorded, or streamed. Their toolbar buttons are gone too.
    //
    // To revive Record Mix in a future release, restore this block (it also
    // brings back the Ctrl+R shortcut). The backing signals/slots still exist:
    //     auto* pOptionsRecord = new QAction(tr("&Record Mix"), this);
    //     pOptionsRecord->setShortcut(QKeySequence(m_pKbdConfig->getValue(
    //             ConfigKey("[KeyboardShortcuts]", "OptionsMenu_RecordMix"),
    //             tr("Ctrl+R"))));
    //     pOptionsRecord->setShortcutContext(Qt::ApplicationShortcut);
    //     pOptionsRecord->setCheckable(true);
    //     connect(pOptionsRecord, &QAction::triggered,
    //             this, &WMainMenuBar::toggleRecording);
    //     connect(this, &WMainMenuBar::internalRecordingStateChange,
    //             pOptionsRecord, &QAction::setChecked);
    //     pOptionsMenu->addAction(pOptionsRecord);
    // (toggleRecording / internalRecordingStateChange are still declared, and the
    // record toolbar widget can be brought back from res/skins/TangoQ/toolbar.xml
    // git history.)

    QString keyboardShortcutTitle = tr("Enable &Keyboard Shortcuts");
    QString keyboardShortcutText = tr("Toggles keyboard shortcuts on or off");
    bool keyboardShortcutsEnabled = m_pConfig->getValueString(
                                            ConfigKey("[Keyboard]", "Enabled")) == "1";
    auto* pOptionsKeyboard = new QAction(keyboardShortcutTitle, this);
    pOptionsKeyboard->setShortcut(
            QKeySequence(m_pKbdConfig->getValue(
                    ConfigKey("[KeyboardShortcuts]", "OptionsMenu_EnableShortcuts"),
                    tr("Ctrl+`"))));
    pOptionsKeyboard->setShortcutContext(Qt::ApplicationShortcut);
    pOptionsKeyboard->setCheckable(true);
    pOptionsKeyboard->setChecked(keyboardShortcutsEnabled);
    pOptionsKeyboard->setStatusTip(keyboardShortcutText);
    pOptionsKeyboard->setWhatsThis(buildWhatsThis(keyboardShortcutTitle, keyboardShortcutText));
    connect(pOptionsKeyboard, &QAction::triggered, this, &WMainMenuBar::toggleKeyboardShortcuts);

    pOptionsMenu->addAction(pOptionsKeyboard);

    pOptionsMenu->addSeparator();

    QString preferencesTitle = tr("&Preferences");
    QString preferencesText = tr("Change %1 settings (e.g. playback, MIDI, controls)").arg(appName);
    auto* pOptionsPreferences = new QAction(preferencesTitle, this);
    pOptionsPreferences->setShortcut(
            QKeySequence(m_pKbdConfig->getValue(
                    ConfigKey("[KeyboardShortcuts]", "OptionsMenu_Preferences"),
                    showPreferencesKeyBinding())));
    pOptionsPreferences->setShortcutContext(Qt::ApplicationShortcut);
    pOptionsPreferences->setStatusTip(preferencesText);
    pOptionsPreferences->setWhatsThis(buildWhatsThis(preferencesTitle, preferencesText));
    pOptionsPreferences->setMenuRole(QAction::PreferencesRole);
    connect(pOptionsPreferences, &QAction::triggered, this, &WMainMenuBar::showPreferences);
    pOptionsMenu->addAction(pOptionsPreferences);

    addMenu(pOptionsMenu);

    // DEVELOPER MENU
    if (CmdlineArgs::Instance().getDeveloper()) {
        QMenu* pDeveloperMenu = new QMenu(tr("&Developer"), this);
#ifndef __APPLE__
        connectMenuToSlotShowMenuBar(pDeveloperMenu);
#endif

        QString reloadSkinTitle = tr("&Reload Skin");
        QString reloadSkinText = tr("Reload the skin");
        auto* pDeveloperReloadSkin = new QAction(reloadSkinTitle, this);
        pDeveloperReloadSkin->setShortcut(
                QKeySequence(m_pKbdConfig->getValue(
                        ConfigKey("[KeyboardShortcuts]", "OptionsMenu_ReloadSkin"),
                        tr("Ctrl+Shift+R"))));
        pDeveloperReloadSkin->setShortcutContext(Qt::ApplicationShortcut);
        pDeveloperReloadSkin->setStatusTip(reloadSkinText);
        pDeveloperReloadSkin->setWhatsThis(buildWhatsThis(reloadSkinTitle, reloadSkinText));
        connect(pDeveloperReloadSkin, &QAction::triggered, this, &WMainMenuBar::reloadSkin);
        pDeveloperMenu->addAction(pDeveloperReloadSkin);

        QString developerToolsTitle = tr("Developer &Tools");
        QString developerToolsText = tr("Opens the developer tools dialog");
        auto* pDeveloperTools = new QAction(developerToolsTitle, this);
        pDeveloperTools->setShortcut(
                QKeySequence(m_pKbdConfig->getValue(
                        ConfigKey("[KeyboardShortcuts]", "OptionsMenu_DeveloperTools"),
                        tr("Ctrl+Shift+T"))));
        pDeveloperTools->setShortcutContext(Qt::ApplicationShortcut);
        pDeveloperTools->setCheckable(true);
        pDeveloperTools->setChecked(false);
        pDeveloperTools->setStatusTip(developerToolsText);
        pDeveloperTools->setWhatsThis(buildWhatsThis(developerToolsTitle, developerToolsText));
        connect(pDeveloperTools, &QAction::triggered, this, &WMainMenuBar::toggleDeveloperTools);
        connect(this,
                &WMainMenuBar::internalDeveloperToolsStateChange,
                pDeveloperTools,
                &QAction::setChecked);
        pDeveloperMenu->addAction(pDeveloperTools);

        QString enableExperimentTitle = tr("Stats: &Experiment Bucket");
        QString enableExperimentToolsText = tr(
                "Enables experiment mode. Collects stats in the EXPERIMENT tracking bucket.");
        auto* pDeveloperStatsExperiment = new QAction(enableExperimentTitle, this);
        pDeveloperStatsExperiment->setShortcut(
                QKeySequence(m_pKbdConfig->getValue(
                        ConfigKey("[KeyboardShortcuts]", "OptionsMenu_DeveloperStatsExperiment"),
                        tr("Ctrl+Shift+E"))));
        pDeveloperStatsExperiment->setShortcutContext(Qt::ApplicationShortcut);
        pDeveloperStatsExperiment->setStatusTip(enableExperimentToolsText);
        pDeveloperStatsExperiment->setWhatsThis(buildWhatsThis(
                enableExperimentTitle, enableExperimentToolsText));
        pDeveloperStatsExperiment->setCheckable(true);
        pDeveloperStatsExperiment->setChecked(Experiment::isExperiment());
        connect(pDeveloperStatsExperiment,
                &QAction::triggered,
                this,
                &WMainMenuBar::slotDeveloperStatsExperiment);
        pDeveloperMenu->addAction(pDeveloperStatsExperiment);

        QString enableBaseTitle = tr("Stats: &Base Bucket");
        QString enableBaseToolsText = tr(
                "Enables base mode. Collects stats in the BASE tracking bucket.");
        auto* pDeveloperStatsBase = new QAction(enableBaseTitle, this);
        pDeveloperStatsBase->setShortcut(
                QKeySequence(m_pKbdConfig->getValue(
                        ConfigKey("[KeyboardShortcuts]", "OptionsMenu_DeveloperStatsBase"),
                        tr("Ctrl+Shift+B"))));
        pDeveloperStatsBase->setShortcutContext(Qt::ApplicationShortcut);
        pDeveloperStatsBase->setStatusTip(enableBaseToolsText);
        pDeveloperStatsBase->setWhatsThis(buildWhatsThis(
                enableBaseTitle, enableBaseToolsText));
        pDeveloperStatsBase->setCheckable(true);
        pDeveloperStatsBase->setChecked(Experiment::isBase());
        connect(pDeveloperStatsBase,
                &QAction::triggered,
                this,
                &WMainMenuBar::slotDeveloperStatsBase);
        pDeveloperMenu->addAction(pDeveloperStatsBase);

        // "D" cannot be used with Alt here as it is already by the Developer menu
        QString scriptDebuggerTitle = tr("Deb&ugger Enabled");
        QString scriptDebuggerText = tr("Enables the debugger during skin parsing");
        bool scriptDebuggerEnabled = m_pConfig->getValueString(
                                             ConfigKey("[ScriptDebugger]", "Enabled")) == "1";
        auto* pDeveloperDebugger = new QAction(scriptDebuggerTitle, this);
        pDeveloperDebugger->setShortcut(
                QKeySequence(m_pKbdConfig->getValue(
                        ConfigKey("[KeyboardShortcuts]", "DeveloperMenu_EnableDebugger"),
                        tr("Ctrl+Shift+D"))));
        pDeveloperDebugger->setShortcutContext(Qt::ApplicationShortcut);
        pDeveloperDebugger->setWhatsThis(buildWhatsThis(keyboardShortcutTitle, keyboardShortcutText));
        pDeveloperDebugger->setCheckable(true);
        pDeveloperDebugger->setStatusTip(scriptDebuggerText);
        pDeveloperDebugger->setChecked(scriptDebuggerEnabled);
        connect(pDeveloperDebugger,
                &QAction::triggered,
                this,
                &WMainMenuBar::slotDeveloperDebugger);
        pDeveloperMenu->addAction(pDeveloperDebugger);

        addMenu(pDeveloperMenu);
    }

    addSeparator();

    // HELP MENU
    QMenu* pHelpMenu = new QMenu(tr("&Help"), this);
#ifndef __APPLE__
    connectMenuToSlotShowMenuBar(pHelpMenu);
#endif

    QString externalLinkSuffix;
#ifndef __APPLE__
    // According to Apple's Human Interface Guidelines devs are encouraged
    // to not use custom icons in menus.
    // https://developer.apple.com/design/human-interface-guidelines/macos/menus/menu-anatomy/
    externalLinkSuffix = QChar(' ') + QChar(0x2197); // north-east arrow
#endif

    // TangoQ: "Community Support" and "User Manual" removed - they point at the
    // upstream Mixxx project, which does not document this fork's Tango workflow.

    // Keyboard Shortcuts
    QUrl keyboardShortcutsUrl = documentationUrl(m_pConfig->getResourcePath(),
            MIXXX_KBD_SHORTCUTS_FILENAME,
            MIXXX_MANUAL_SHORTCUTS_URL);
    QString keyboardShortcutsSuffix =
            keyboardShortcutsUrl.isLocalFile() ? QString() : externalLinkSuffix;

    QString shortcutsTitle = tr("&Keyboard Shortcuts") + keyboardShortcutsSuffix;
    QString shortcutsText = tr("Speed up your workflow with keyboard shortcuts.");
    auto* pHelpKbdShortcuts = new QAction(shortcutsTitle, this);
    pHelpKbdShortcuts->setStatusTip(shortcutsText);
    pHelpKbdShortcuts->setWhatsThis(buildWhatsThis(shortcutsTitle, shortcutsText));
    connect(pHelpKbdShortcuts,
            &QAction::triggered,
            this,
            [this, keyboardShortcutsUrl] {
                slotVisitUrl(keyboardShortcutsUrl);
            });
    pHelpMenu->addAction(pHelpKbdShortcuts);

    // User Settings Directory
    const QString& settingsDirPath = m_pConfig->getSettingsPath();
    QString settingsDirTitle = tr("&Settings directory");
    QString settingsDirText = tr("Open the %1 user settings directory.").arg(appName);
    auto* pHelpSettingsDir = new QAction(settingsDirTitle, this);
    pHelpSettingsDir->setMenuRole(QAction::NoRole);
    pHelpSettingsDir->setStatusTip(settingsDirText);
    pHelpSettingsDir->setWhatsThis(buildWhatsThis(settingsDirTitle, settingsDirText));
    connect(pHelpSettingsDir, &QAction::triggered, this, [this, settingsDirPath] {
        slotVisitUrl(QUrl::fromLocalFile(settingsDirPath));
    });
    pHelpMenu->addAction(pHelpSettingsDir);

    // TangoQ: "Translate This Application" removed - it points at the upstream
    // Mixxx translation project, not this fork.

    pHelpMenu->addSeparator();

    QString aboutTitle = tr("&About");
    QString aboutText = tr("About the application");
    auto* pHelpAboutApp = new QAction(aboutTitle, this);
    pHelpAboutApp->setStatusTip(aboutText);
    pHelpAboutApp->setWhatsThis(buildWhatsThis(aboutTitle, aboutText));
    pHelpAboutApp->setMenuRole(QAction::AboutRole);
    connect(pHelpAboutApp, &QAction::triggered, this, &WMainMenuBar::showAbout);

    pHelpMenu->addAction(pHelpAboutApp);
    addMenu(pHelpMenu);

#ifndef __APPLE__
    // Watch focus changes to hide the menubar as soon as all menus are closed,
    // e.g. when an action was triggered or when all menus are closed by pressing
    // Escape or clicking anywhere else
    connect(qApp,
            &QApplication::focusWindowChanged,
            this,
            [this]() {
                if (!isNativeMenuBar() && height() > 0 && !activeAction()) {
                    hideMenuBar();
                }
            });
    // ... and when the focus widget changes (main window or dialogs)
    connect(qApp,
            // This would work, too, but unfortunately this is also emitted on
            // leaveEvent of WStarRating in the library.
            // &QApplication::focusObjectChanged,
            &QApplication::focusChanged,
            this,
            [this]() {
                if (!isNativeMenuBar() && height() > 0 && !activeAction()) {
                    hideMenuBar();
                }
            });
#endif
}

void WMainMenuBar::onKeywheelChange(int state) {
    Q_UNUSED(state);
    m_pViewKeywheel->setChecked(false);
}

void WMainMenuBar::onLibraryScanStarted() {
    emit internalLibraryScanActive(true);
}

void WMainMenuBar::onLibraryScanFinished() {
    emit internalLibraryScanActive(false);
}

void WMainMenuBar::onNewSkinLoaded() {
    emit internalOnNewSkinLoaded();
}

void WMainMenuBar::onNewSkinAboutToLoad() {
    emit internalOnNewSkinAboutToLoad();
}

void WMainMenuBar::onRecordingStateChange(bool recording) {
    emit internalRecordingStateChange(recording);
}

void WMainMenuBar::onBroadcastingStateChange(bool broadcasting) {
    emit internalBroadcastingStateChange(broadcasting);
}

void WMainMenuBar::onDeveloperToolsShown() {
    emit internalDeveloperToolsStateChange(true);
}

void WMainMenuBar::onDeveloperToolsHidden() {
    emit internalDeveloperToolsStateChange(false);
}

void WMainMenuBar::onFullScreenStateChange(bool fullscreen) {
#ifndef __APPLE__
    // always try to hide the menubar when we switched
    hideMenuBar();
#endif
    emit internalFullScreenStateChange(fullscreen);
}

#ifndef __APPLE__
void WMainMenuBar::connectMenuToSlotShowMenuBar(const QMenu* pMenu) {
    // If a menu hotkey like Alt+F(ile) is pressed while the menubar is hidden,
    // show the menubar and open the requested menu. See showMenuBar() for details.

    // NOTE(ronso0) Test with xfwm4 and other window managers if you change this
    // code or think you found alternative ways to toggle the menubar!
    // In Gnome for example, when highlighting (pressed Alt only) or activating
    // menus (Alt combos), the menubar receives focusIn events (even while it is
    // hidden), and focusOut events respectively when closing menus. Hence we
    // could simply show/hide the menubar in QMenuBar::focusInEvent() and focusoutEvent().
    // However, with xfwm4 for example, menus are activated directly, i.e. the
    // menubar doesn't receive focus change events.
    connect(pMenu,
            &QMenu::aboutToShow,
            this,
            [this]() {
                if (!isNativeMenuBar() && height() <= 0) {
                    showMenuBar();
                }
            });
}

void WMainMenuBar::slotToggleMenuBar() {
    if (isNativeMenuBar()) {
        return;
    }

    if (height() > 0) {
        hideMenuBar();
    } else {
        showMenuBar();
    }
}

void WMainMenuBar::showMenuBar() {
    if (isNativeMenuBar()) {
        return;
    }
    // Note: the resulting resizeEvent() calls QMenuBarPrivate::updateGeometries
    // which resets currentAction() to nullptr. So in case the menubar is shown
    // in response to a specific menu hotkey (Alt-F), or pressing Alt opens the
    // first menu (File) (depends on OS / window manager), the current action
    // will be reset to null and menus can't be switched with Left/Right keys
    // anymore, i.e. we'd be stuck in the triggered menu.
    // Workaround: reselect the active action after unhiding. It can be none,
    // user-requested (hotkey) or auto-selected (first menu's first action).
    QAction* pAct = activeAction();
    setMinimumHeight(sizeHint().height());
    // If there was a menu selected before, reselect that.
    // Note: with nullptr this would be a no-op. Though, even if no menu is
    // selected, hovering a menu would instantly open that (no click required).
    if (pAct) {
        setActiveAction(pAct);
    }
    // TODO Alternatively, activate the first menu?
    // setActiveAction(pAct ? pAct : actions().first());
}

void WMainMenuBar::hideMenuBar() {
    if (isNativeMenuBar()) {
        return;
    }
    if (m_pConfig->getValue<bool>(ConfigKey("[Config]", "hide_menubar"), false)) {
        // don't use setHidden(true) because Alt hotkeys wouldn't work anymore
        setFixedHeight(0);
    }
}

void WMainMenuBar::slotAutoHideMenuBarToggled(bool autoHide) {
    m_pConfig->setValue(ConfigKey("[Config]", "hide_menubar"), autoHide ? 1 : 0);
    // Trigger slotUpdateMenuBarAltKeyConnection() inorder to get Alt work immediately
    emit menubarAutoHideChanged(autoHide);
    // Just in case it was hidden after toggling the menu action
    if (!autoHide) {
        showMenuBar();
    }
}
#endif

void WMainMenuBar::onVinylControlDeckEnabledStateChange(int deck, bool enabled) {
    VERIFY_OR_DEBUG_ASSERT(deck >= 0 && deck < m_vinylControlEnabledActions.size()) {
        return;
    }
    m_vinylControlEnabledActions.at(deck)->setChecked(enabled);
}

void WMainMenuBar::slotDeveloperStatsBase(bool enable) {
    if (enable) {
        Experiment::setBase();
    } else {
        Experiment::disable();
    }
}

void WMainMenuBar::slotDeveloperStatsExperiment(bool enable) {
    if (enable) {
        Experiment::setExperiment();
    } else {
        Experiment::disable();
    }
}

void WMainMenuBar::slotDeveloperDebugger(bool toggle) {
    m_pConfig->set(ConfigKey("[ScriptDebugger]", "Enabled"),
            ConfigValue(toggle ? 1 : 0));
}

void WMainMenuBar::slotVisitUrl(const QUrl& url) {
    mixxx::DesktopHelper::openUrl(url);
}

void WMainMenuBar::createVisibilityControl(QAction* pAction,
        const ConfigKey& key,
        const ConfigKey& gateKey) {
    auto* pConnection = new VisibilityControlConnection(this, pAction, key, gateKey);
    connect(this,
            &WMainMenuBar::internalOnNewSkinLoaded,
            pConnection,
            &VisibilityControlConnection::slotReconnectControl);
#ifdef __LINUX__
    // reconnect when menu bar was recreated after toggling fullscreen
    // so all hotkeys and menu actions continue to work
    connect(this,
            &WMainMenuBar::internalFullScreenStateChange,
            pConnection,
            &VisibilityControlConnection::slotReconnectControl);
#endif
    connect(this,
            &WMainMenuBar::internalOnNewSkinAboutToLoad,
            pConnection,
            &VisibilityControlConnection::slotClearControl);
}

void WMainMenuBar::onNumberOfDecksChanged(int decks) {
    // TangoQ: only two decks are ever shown, even though the engine may still be
    // configured for four. Cap the File > Load to Deck actions (and the vinyl
    // actions, if any survive) at two so decks 3 and 4 - and their load shortcuts
    // - never appear.
    const int visibleDecks = std::min(decks, 2);
    int deck = 0;
    for (QAction* pVinylControlEnabled : std::as_const(m_vinylControlEnabledActions)) {
        pVinylControlEnabled->setVisible(deck++ < visibleDecks);
    }
    deck = 0;
    for (QAction* pLoadToDeck : std::as_const(m_loadToDeckActions)) {
        const bool show = deck++ < visibleDecks;
        pLoadToDeck->setVisible(show);
        pLoadToDeck->setEnabled(show);
    }
}

VisibilityControlConnection::VisibilityControlConnection(
        QObject* pParent, QAction* pAction, const ConfigKey& key, const ConfigKey& gateKey)
        : QObject(pParent),
          m_key(key),
          m_gateKey(gateKey),
          m_pAction(pAction) {
    connect(m_pAction, &QAction::triggered, this, &VisibilityControlConnection::slotActionToggled);
}

void VisibilityControlConnection::slotClearControl() {
    m_pControl.reset();
    m_pGateControl.reset();
    m_pAction->setEnabled(false);
}

void VisibilityControlConnection::slotReconnectControl() {
    m_pControl.reset(new ControlProxy(m_key, this, ControlFlag::NoAssertIfMissing));
    m_pControl->connectValueChanged(this, &VisibilityControlConnection::slotControlChanged);
    if (m_gateKey.isValid()) {
        m_pGateControl.reset(new ControlProxy(m_gateKey, this, ControlFlag::NoAssertIfMissing));
        m_pGateControl->connectValueChanged(this, &VisibilityControlConnection::slotGateChanged);
    }
    updateActionState();
    slotControlChanged();
}

void VisibilityControlConnection::updateActionState() {
    const bool controlAvailable = m_pControl && m_pControl->valid();
    const bool gateOpen = !m_pGateControl || m_pGateControl->toBool();
    m_pAction->setEnabled(controlAvailable && gateOpen);
    // A gated action (e.g. Auto DJ Queue) is hidden entirely while its gate
    // (Tango mode) is closed, so the menu is unchanged outside that mode rather
    // than showing a greyed-out item. Disable it too so its shortcut cannot
    // toggle the hidden feature while the gate is closed.
    if (m_pGateControl) {
        m_pAction->setVisible(gateOpen);
    }
}

void VisibilityControlConnection::slotGateChanged() {
    updateActionState();
}

void VisibilityControlConnection::slotControlChanged() {
    if (m_pControl) {
        m_pAction->setChecked(m_pControl->toBool());
    }
}

void VisibilityControlConnection::slotActionToggled(bool toggle) {
    if (m_pControl) {
        m_pControl->set(toggle ? 1.0 : 0.0);
    }
}
