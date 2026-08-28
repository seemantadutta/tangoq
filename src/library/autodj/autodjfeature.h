#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QUuid>
#include <QVariant>
#include <QVector>
#include <memory>

#include "control/controlpushbutton.h"
#include "library/dao/autodjcratesdao.h"
#include "library/libraryfeature.h"
#include "library/trackset/crate/crate.h"
#include "preferences/usersettings.h"
#include "util/parented_ptr.h"

class DlgAutoDJ;
class Library;
class WLibrary;
class QDockWidget;
class ControlProxy;
class PlayerManagerInterface;
class TrackCollection;
class AutoDJProcessor;
class WLibrarySidebar;
class QAction;
class QModelIndex;
class QPoint;
class TandaQueueState;
enum class TandaType;
namespace mixxx::semanticstate {
class Store;
class TangoQAdapter;
class Server;
} // namespace mixxx::semanticstate

class AutoDJFeature : public LibraryFeature {
    Q_OBJECT
  public:
    AutoDJFeature(Library* pLibrary,
            UserSettingsPointer pConfig,
            PlayerManagerInterface* pPlayerManager);
    virtual ~AutoDJFeature();

    QVariant title() override;

    void clear() override;
    void paste() override;
    void deleteItem(const QModelIndex& index) override;

    bool dropAccept(const QList<QUrl>& urls, QObject* pSource) override;
    bool dragMoveAccept(const QUrl& url) override;

    void bindLibraryWidget(WLibrary* libraryWidget,
            KeyboardEventFilter* keyboard) override;
    void bindSidebarWidget(WLibrarySidebar* pSidebarWidget) override;

    // Builds the detached/dockable Auto DJ queue panel (a second view of the
    // queue model). Owned by Qt via the passed parent (the main window); the
    // feature keeps only a QPointer. Placed by MixxxMainWindow.
    QDockWidget* createAutoDJDockWidget(QWidget* parent);

    TreeItemModel* sidebarModel() const override;

    TandaQueueState* tandaQueueState() const {
        return m_pTandaQueueState.get();
    }
    QUuid makeTanda(const QVector<int>& oneBasedPositions,
            TandaType type,
            QString* pError = nullptr);
    bool ungroupTanda(const QUuid& id);
    bool changeTandaType(const QUuid& id, TandaType type);
    bool setTandaCollapsed(const QUuid& id, bool collapsed);
    bool moveTanda(const QUuid& id,
            int newAnchorPosition,
            QString* pError = nullptr);
    bool moveTandaUp(const QUuid& id, QString* pError = nullptr);
    bool moveTandaDown(const QUuid& id, QString* pError = nullptr);

    bool hasTrackTable() override {
        return true;
    }

  public slots:
    void activate() override;

    void onRightClick(const QPoint& globalPos) override;
    // Temporary, until WCrateTableView can be written.
    void onRightClickChild(const QPoint& globalPos, const QModelIndex& index) override;

  private:
    TrackCollection* const m_pTrackCollection;

    PlaylistDAO& m_playlistDao;
    // The id of the AutoDJ playlist.
    int m_iAutoDJPlaylistId;
    AutoDJProcessor* m_pAutoDJProcessor;
    std::unique_ptr<TandaQueueState> m_pTandaQueueState;
    bool m_tandaMoveInProgress{false};
    parented_ptr<TreeItemModel> m_pSidebarModel;
    DlgAutoDJ* m_pAutoDJView;

    // Toggle control for the dockable Auto DJ queue panel ([AutoDJ],
    // show_autodj_dock). The View menu binds to it; the dock follows it and
    // writes it back when the user closes the dock.
    ControlPushButton m_showAutoDJDockControl;
    // Observes Tango mode ([AutoDJ],keep_queue, owned by the processor). The
    // queue panel is gated behind it. Parented to this; Qt owns it.
    ControlProxy* m_pTangoModeControl;
    // The dockable queue panel. Owned by Qt (parented to the main window); we
    // hold a QPointer so it auto-nulls if the window is destroyed first.
    QPointer<QDockWidget> m_pAutoDJDock;
    // The docked library widget, captured in bindLibraryWidget(). Used as the
    // source of the skin's library stylesheet for the dockable queue panel.
    QPointer<WLibrary> m_pLibraryWidget;
    // The skin's library stylesheet, so the dockable queue panel's track table
    // matches the docked Auto DJ view.
    QString libraryStyleSheet() const;
    // True while Tango mode ([AutoDJ],keep_queue) is engaged.
    bool tangoModeEnabled() const;
    // Cached Tango-mode state. This avoids re-querying the keep_queue proxy
    // while its own valueChanged signal is still being processed.
    bool m_tangoModeEnabled;
    // True when Tango mode forced the Auto DJ side panel closed and it should
    // be restored the next time Tango mode is enabled.
    bool m_restoreAutoDJDockOnTangoMode;

    // Initialize the list of crates loaded into the auto-DJ queue.
    void constructCrateChildModel();
    void removeCrateFromAutoDj(CrateId crateId = CrateId());

    // The "Crates" tree-item under the "Auto DJ" tree-item.
    TreeItem* m_pCratesTreeItem;

    // The crate ID and name of all loaded crates.
    // Its indices correspond one-to-one with tree-items contained by the
    // "Crates" tree-item.
    QList<Crate> m_crateList;

    // How we access the auto-DJ-crates database.
    AutoDJCratesDAO m_autoDjCratesDao;

    parented_ptr<QAction> m_pClearQueueAction;
    // A context-menu item that allows crates to be removed from the
    // auto-DJ list.
    parented_ptr<QAction> m_pRemoveCrateFromAutoDjAction;

    QPointer<WLibrarySidebar> m_pSidebarWidget;

    // Experimental, read-only semantic monitor. These are created only when
    // --semantic-monitor-port is present and are destroyed before AutoDJ state.
    std::unique_ptr<mixxx::semanticstate::Store> m_pSemanticStateStore;
    std::unique_ptr<mixxx::semanticstate::TangoQAdapter> m_pSemanticStateAdapter;
    std::unique_ptr<mixxx::semanticstate::Server> m_pSemanticStateServer;

  private slots:
    // Reacts to the [AutoDJ],show_autodj_dock control (from the View menu).
    void slotShowAutoDJDockChanged(double value);
    // Reacts to Tango mode ([AutoDJ],keep_queue) turning on/off; hides the queue
    // panel when Tango mode is left.
    void slotTangoModeChanged(double value);
    // Mirrors the dock's user-driven show/hide back into the control (so the
    // View menu check follows the dock's close button), ignoring visibility
    // changes caused by the main window being minimized.
    void slotAutoDJDockVisibilityChanged(bool visible);
    void slotClearQueue();
    // Add a crate to the auto-DJ queue.
    void slotAddCrateToAutoDj(CrateId crateId);
    // Implements the context-menu item.
    void slotRemoveCrateFromAutoDj();
    void slotCrateChanged(CrateId crateId);
    // Adds a random track from all loaded crates to the auto-DJ queue.
    void slotAddRandomTrack();
    // Adds a random track from the queue upon hitting minimum number
    // of tracks in the playlist
    void slotRandomQueue(int numTracksToAdd);
};
