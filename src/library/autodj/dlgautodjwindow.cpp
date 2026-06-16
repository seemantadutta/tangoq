#include "library/autodj/dlgautodjwindow.h"

#include <QCloseEvent>
#include <QVBoxLayout>

#include "library/playlisttablemodel.h"
#include "moc_dlgautodjwindow.cpp"
#include "widget/wtracktableview.h"

DlgAutoDJWindow::DlgAutoDJWindow(UserSettingsPointer pConfig,
        Library* pLibrary,
        PlaylistTableModel* pModel)
        // Independent top-level window (no parent); the owning AutoDJFeature keeps
        // and deletes it.
        : QWidget(nullptr, Qt::Window),
          m_pTrackTableView(new WTrackTableView(this, pConfig, pLibrary, 1.0)) {
    setWindowTitle(tr("Auto DJ Queue"));
    setObjectName(QStringLiteral("AutoDJWindow"));
    // Don't let this secondary window keep Mixxx alive after the main window
    // closes (quit is driven by the main window).
    setAttribute(Qt::WA_QuitOnClose, false);

    auto* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->addWidget(m_pTrackTableView);

    // Second view onto the shared Auto DJ model: live, with the same styling.
    m_pTrackTableView->loadTrackModel(pModel);

    connect(m_pTrackTableView,
            &WTrackTableView::loadTrack,
            this,
            &DlgAutoDJWindow::loadTrack);
    connect(m_pTrackTableView,
            &WTrackTableView::loadTrackToPlayer,
            this,
            &DlgAutoDJWindow::loadTrackToPlayer);

    resize(520, 420);
}

DlgAutoDJWindow::~DlgAutoDJWindow() = default;

void DlgAutoDJWindow::closeEvent(QCloseEvent* pEvent) {
    emit closed();
    QWidget::closeEvent(pEvent);
}
