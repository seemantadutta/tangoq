#pragma once

#include <QList>
#include <QTimer>
#include <QUuid>

#include "widget/wtracktableview.h"

class QAction;
class AutoDJFeature;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMenu;
class TandaQueueModel;
enum class TandaType;

/// Track table behavior for the Tango Auto DJ outline.
///
/// Virtual header rows are command targets only. Ordinary track rows continue
/// through WTrackTableView and retain its loading, editing, drag/drop, and track
/// context-menu behavior.
class WTandaQueueView final : public WTrackTableView {
    Q_OBJECT

  public:
    WTandaQueueView(QWidget* pParent,
            UserSettingsPointer pConfig,
            Library* pLibrary,
            double backgroundColorOpacity,
            AutoDJFeature* pAutoDJFeature);

  protected:
    void contextMenuEvent(QContextMenuEvent* pEvent) override;
    void mousePressEvent(QMouseEvent* pEvent) override;
    void mouseDoubleClickEvent(QMouseEvent* pEvent) override;
    void mouseMoveEvent(QMouseEvent* pEvent) override;
    void dragEnterEvent(QDragEnterEvent* pEvent) override;
    void dragMoveEvent(QDragMoveEvent* pEvent) override;
    void dropEvent(QDropEvent* pEvent) override;
    void keyPressEvent(QKeyEvent* pEvent) override;
    void paintEvent(QPaintEvent* pEvent) override;
    void prepareTrackMenu(
            WTrackMenu* pTrackMenu, const QModelIndexList& indices) override;

  private:
    TandaQueueModel* tandaModel() const;
    bool selectedRowsAreHeaders() const;
    QVector<int> selectedQueuePositions(bool* pAllLeaves = nullptr) const;
    bool canClassifySelection() const;
    bool selectionContainsTandaLeaves() const;
    void classifySelection(TandaType type);
    void setContextTanda(const QUuid& id);
    void toggleTanda(const QUuid& id);
    int destinationAnchorForDrop(const QPoint& position, const QUuid& id) const;
    void moveContextTanda(bool up);
    void showTandaHeaderMenu(const QPoint& globalPos, const QUuid& id);
    void showError(const QString& message);

    AutoDJFeature* const m_pAutoDJFeature;
    QUuid m_contextTandaId;
    QList<QAction*> m_classifyActions;
    QMenu* m_pChangeTandaTypeMenu;
    QAction* m_pTandaSeparator;
    QAction* m_pToggleCollapsedAction;
    QAction* m_pUngroupAction;
    QAction* m_pMoveUpAction;
    QAction* m_pMoveDownAction;
    QUuid m_pressedHeaderTandaId;
    QTimer m_pauseBlinkTimer;
    bool m_pauseBlinkOn{true};
};
