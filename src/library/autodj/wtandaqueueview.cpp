#include "library/autodj/wtandaqueueview.h"

#include <algorithm>
#include <utility>

#include <QAction>
#include <QBrush>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontMetrics>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSet>

#include "library/autodj/autodjfeature.h"
#include "library/autodj/cortinaregistry.h"
#include "library/autodj/tandaqueuemodel.h"
#include "library/autodj/tandaqueuestate.h"
#include "library/playlisttablemodel.h"
#include "moc_wtandaqueueview.cpp"
#include "util/dnd.h"
#include "widget/wtrackmenu.h"

namespace {

const char* kTandaHeaderMimeType = "application/x-mixxx-tanda-header";
constexpr int kDisclosureHitWidth = 24;
constexpr int kProgressPipDiameter = 8;
constexpr int kProgressPipGap = 5;

QUuid tandaIdFromMimeData(const QMimeData* pMimeData) {
    if (!pMimeData || !pMimeData->hasFormat(kTandaHeaderMimeType)) {
        return {};
    }
    return QUuid(QString::fromUtf8(pMimeData->data(kTandaHeaderMimeType)));
}

QColor colorFromForegroundRole(const QVariant& foreground, const QColor& fallback) {
    if (foreground.canConvert<QBrush>()) {
        return foreground.value<QBrush>().color();
    }
    if (foreground.canConvert<QColor>()) {
        return foreground.value<QColor>();
    }
    return fallback;
}

void drawProgressPip(
        QPainter* pPainter, const QRectF& rect, const QColor& color, QChar state) {
    QPen pen(color);
    pen.setWidth(1);
    pPainter->setPen(pen);
    pPainter->setBrush(Qt::NoBrush);
    pPainter->drawEllipse(rect);

    if (state == QLatin1Char('1')) {
        pPainter->setBrush(color);
        pPainter->drawEllipse(rect);
    } else if (state == QLatin1Char('h')) {
        pPainter->setBrush(color);
        pPainter->drawPie(rect, 90 * 16, 180 * 16);
    }

    pPainter->setBrush(Qt::NoBrush);
    pPainter->drawEllipse(rect);
}

} // namespace

WTandaQueueView::WTandaQueueView(QWidget* pParent,
        UserSettingsPointer pConfig,
        Library* pLibrary,
        double backgroundColorOpacity,
        AutoDJFeature* pAutoDJFeature)
        : WTrackTableView(pParent,
                  std::move(pConfig),
                  pLibrary,
                  backgroundColorOpacity),
          m_pAutoDJFeature(pAutoDJFeature),
          m_pChangeTandaTypeMenu(new QMenu(tr("Change tanda type"), this)),
          m_pTandaSeparator(new QAction(this)),
          m_pToggleCollapsedAction(new QAction(this)),
          m_pUngroupAction(new QAction(tr("Unmake Tanda"), this)),
          m_pRemoveAction(new QAction(tr("Remove"), this)),
          m_pMoveUpAction(new QAction(tr("Move tanda up"), this)),
          m_pMoveDownAction(new QAction(tr("Move tanda down"), this)) {
    m_pauseBlinkTimer.setInterval(500);
    connect(&m_pauseBlinkTimer, &QTimer::timeout, this, [this] {
        m_pauseBlinkOn = !m_pauseBlinkOn;
        viewport()->update();
    });
    m_pauseBlinkTimer.start();

    m_pTandaSeparator->setSeparator(true);

    const QList<std::pair<QString, TandaType>> classifyTypes = {
            {tr("Make Tango tanda"), TandaType::Tango},
            {tr("Make Vals tanda"), TandaType::Vals},
            {tr("Make Milonga tanda"), TandaType::Milonga},
            {tr("Make Nuevo / Alternative tanda"), TandaType::NuevoAlternative},
    };
    for (const auto& entry : classifyTypes) {
        QAction* pAction = new QAction(entry.first, this);
        m_classifyActions.append(pAction);
        connect(pAction, &QAction::triggered, this, [this, type = entry.second] {
            classifySelection(type);
        });
    }

    const auto addChangeTypeActions = [this] {
        const QList<std::pair<QString, TandaType>> types = {
                {tr("Tango"), TandaType::Tango},
                {tr("Vals"), TandaType::Vals},
                {tr("Milonga"), TandaType::Milonga},
                {tr("Nuevo / Alternative"), TandaType::NuevoAlternative},
        };
        for (const auto& entry : types) {
            QAction* pAction = m_pChangeTandaTypeMenu->addAction(entry.first);
            connect(pAction, &QAction::triggered, this, [this, type = entry.second] {
                if (!m_contextTandaId.isNull()) {
                    m_pAutoDJFeature->changeTandaType(m_contextTandaId, type);
                }
            });
        }
    };
    addChangeTypeActions();

    connect(m_pToggleCollapsedAction, &QAction::triggered, this, [this] {
        toggleTanda(m_contextTandaId);
    });
    connect(m_pUngroupAction, &QAction::triggered, this, [this] {
        if (!m_contextTandaId.isNull()) {
            m_pAutoDJFeature->ungroupTanda(m_contextTandaId);
        }
    });
    connect(m_pRemoveAction, &QAction::triggered, this, [this] {
        removeSelectedTracks();
    });
    connect(m_pMoveUpAction, &QAction::triggered, this, [this] {
        moveContextTanda(true);
    });
    connect(m_pMoveDownAction, &QAction::triggered, this, [this] {
        moveContextTanda(false);
    });
}

void WTandaQueueView::contextMenuEvent(QContextMenuEvent* pEvent) {
    TandaQueueModel* pModel = tandaModel();
    const QModelIndex clicked = indexAt(pEvent->pos());
    const int row = rowAt(pEvent->pos().y());
    if (!pModel || row < 0) {
        WTrackTableView::contextMenuEvent(pEvent);
        return;
    }

    if (pModel->isHeaderRow(row)) {
        const QModelIndex headerIndex = pModel->index(row,
                clicked.isValid() ? clicked.column() : pModel->summaryColumn());
        if (!selectionModel()->isRowSelected(row, QModelIndex())) {
            selectionModel()->clearSelection();
            selectionModel()->select(headerIndex,
                    QItemSelectionModel::Select | QItemSelectionModel::Rows);
            setCurrentIndex(headerIndex);
        }
        pEvent->accept();
        showTandaHeaderMenu(
                pEvent->globalPos(), pModel->tandaIdForRow(row));
        return;
    }

    WTrackTableView::contextMenuEvent(pEvent);
}

void WTandaQueueView::mousePressEvent(QMouseEvent* pEvent) {
    TandaQueueModel* pModel = tandaModel();
    const int row = rowAt(pEvent->pos().y());
    m_pressedHeaderTandaId = {};
    const int summaryLeft = pModel ? columnViewportPosition(pModel->summaryColumn()) : -1;
    if (pEvent->button() == Qt::LeftButton && pModel &&
            pModel->isHeaderRow(row) && summaryLeft >= 0 &&
            pEvent->pos().x() >= summaryLeft &&
            pEvent->pos().x() < summaryLeft + kDisclosureHitWidth) {
        toggleTanda(pModel->tandaIdForRow(row));
        pEvent->accept();
        return;
    }
    if (pEvent->button() == Qt::LeftButton && pModel &&
            pModel->isHeaderRow(row)) {
        m_pressedHeaderTandaId = pModel->tandaIdForRow(row);
    }
    WTrackTableView::mousePressEvent(pEvent);
}

void WTandaQueueView::mouseDoubleClickEvent(QMouseEvent* pEvent) {
    TandaQueueModel* pModel = tandaModel();
    const int row = rowAt(pEvent->pos().y());
    if (pModel && pModel->isHeaderRow(row)) {
        toggleTanda(pModel->tandaIdForRow(row));
        pEvent->accept();
        return;
    }
    WTrackTableView::mouseDoubleClickEvent(pEvent);
}

void WTandaQueueView::mouseMoveEvent(QMouseEvent* pEvent) {
    if (!m_pressedHeaderTandaId.isNull() &&
            pEvent->buttons() == Qt::LeftButton &&
            DragAndDropHelper::mouseMoveInitiatesDrag(pEvent)) {
        auto* pDrag = new QDrag(this);
        auto* pMimeData = new QMimeData;
        pMimeData->setData(kTandaHeaderMimeType,
                m_pressedHeaderTandaId.toString(QUuid::WithoutBraces).toUtf8());
        pDrag->setMimeData(pMimeData);
        pDrag->exec(Qt::MoveAction);
        pEvent->accept();
        return;
    }
    WTrackTableView::mouseMoveEvent(pEvent);
}

void WTandaQueueView::dragEnterEvent(QDragEnterEvent* pEvent) {
    if (pEvent->source() == this &&
            !tandaIdFromMimeData(pEvent->mimeData()).isNull()) {
        pEvent->acceptProposedAction();
        return;
    }
    WTrackTableView::dragEnterEvent(pEvent);
}

void WTandaQueueView::dragMoveEvent(QDragMoveEvent* pEvent) {
    if (pEvent->source() == this &&
            !tandaIdFromMimeData(pEvent->mimeData()).isNull()) {
        pEvent->acceptProposedAction();
        return;
    }
    WTrackTableView::dragMoveEvent(pEvent);
}

void WTandaQueueView::dropEvent(QDropEvent* pEvent) {
    const QUuid id = tandaIdFromMimeData(pEvent->mimeData());
    if (pEvent->source() != this || id.isNull()) {
        WTrackTableView::dropEvent(pEvent);
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint position = pEvent->position().toPoint();
#else
    const QPoint position = pEvent->pos();
#endif
    const int destination = destinationAnchorForDrop(position, id);
    if (destination <= 0) {
        pEvent->ignore();
        return;
    }
    QString error;
    if (!m_pAutoDJFeature->moveTanda(id, destination, &error) &&
            !error.isEmpty()) {
        showError(error);
    }
    pEvent->acceptProposedAction();
}

void WTandaQueueView::keyPressEvent(QKeyEvent* pEvent) {
    TandaQueueModel* pModel = tandaModel();
    const QModelIndex current = currentIndex();
    if (pModel && current.isValid() && pModel->isHeaderRow(current.row())) {
        const QUuid id = pModel->tandaIdForRow(current.row());
        if (pEvent->key() == Qt::Key_Return || pEvent->key() == Qt::Key_Enter ||
                pEvent->key() == Qt::Key_Space) {
            toggleTanda(id);
            pEvent->accept();
            return;
        }
        if (pEvent->modifiers().testFlag(Qt::AltModifier) &&
                (pEvent->key() == Qt::Key_Up ||
                        pEvent->key() == Qt::Key_Down)) {
            setContextTanda(id);
            moveContextTanda(pEvent->key() == Qt::Key_Up);
            pEvent->accept();
            return;
        }
        if (pEvent->matches(QKeySequence::Delete) ||
                pEvent->key() == Qt::Key_Backspace) {
            removeSelectedTracks();
            pEvent->accept();
            return;
        }
        if (pEvent->matches(QKeySequence::Cut) ||
                pEvent->matches(QKeySequence::Copy)) {
            pEvent->accept();
            return;
        }
    }
    WTrackTableView::keyPressEvent(pEvent);
}

void WTandaQueueView::paintEvent(QPaintEvent* pEvent) {
    WTrackTableView::paintEvent(pEvent);

    TandaQueueModel* pModel = tandaModel();
    if (!pModel) {
        return;
    }
    const int firstRow = rowAt(0);
    if (firstRow < 0) {
        return;
    }
    int lastRow = rowAt(viewport()->height() - 1);
    if (lastRow < 0) {
        lastRow = pModel->rowCount() - 1;
    }
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    const int disclosureLeft = columnViewportPosition(pModel->summaryColumn());
    for (int row = firstRow; row <= lastRow; ++row) {
        if (!pModel->isHeaderRow(row)) {
            continue;
        }
        const TandaSpan* pSpan =
                m_pAutoDJFeature->tandaQueueState()->spanById(
                        pModel->tandaIdForRow(row));
        if (!pSpan) {
            continue;
        }
        const int y = rowViewportPosition(row);
        const int height = rowHeight(row);
        const int centerY = y + height / 2;
        const QModelIndex summaryIndex = pModel->index(row, pModel->summaryColumn());
        const QColor headerColor = colorFromForegroundRole(
                pModel->data(summaryIndex, Qt::ForegroundRole),
                QColor(225, 230, 236));
        painter.setPen(QPen(headerColor));
        painter.setBrush(QBrush(headerColor));
        const int x = disclosureLeft >= 0 ? disclosureLeft + 8 : 8;
        QPolygon triangle;
        if (pSpan->collapsed) {
            triangle << QPoint(x, centerY - 5)
                     << QPoint(x, centerY + 5)
                     << QPoint(x + 6, centerY);
        } else {
            triangle << QPoint(x - 2, centerY - 3)
                     << QPoint(x + 8, centerY - 3)
                     << QPoint(x + 3, centerY + 4);
        }
        painter.drawPolygon(triangle);

        const QString progressStates = pModel->tandaProgressStatesForRow(row);
        if (!progressStates.isEmpty()) {
            QFont headerFont = font();
            headerFont.setBold(true);
            const QFontMetrics fontMetrics(headerFont);
            const QString label =
                    pModel->data(summaryIndex, Qt::DisplayRole).toString();
            int pipLeft = columnViewportPosition(pModel->summaryColumn()) +
                    fontMetrics.horizontalAdvance(label) + 10;
            const int pipTop = centerY - kProgressPipDiameter / 2;
            for (const QChar state : progressStates) {
                drawProgressPip(&painter,
                        QRectF(pipLeft,
                                pipTop,
                                kProgressPipDiameter,
                                kProgressPipDiameter),
                        headerColor,
                        state);
                pipLeft += kProgressPipDiameter + kProgressPipGap;
            }
        }
    }

    if (!pModel->playlistModel()->showCortinaMarks()) {
        return;
    }
    QPen pausePen(QColor(0xb6, 0x54, 0x54));
    pausePen.setWidth(2);
    QPen activePausePen(QColor(0xff, 0x66, 0x66));
    activePausePen.setWidth(3);
    for (int row = firstRow; row <= lastRow; ++row) {
        const int sourceRow = pModel->sourceRowForVisibleRow(row);
        if (sourceRow < 0) {
            continue;
        }
        const bool activePause =
                pModel->playlistModel()->isActivePauseAfterRow(sourceRow);
        if (!pModel->playlistModel()->isPauseAfterRow(sourceRow) &&
                !(activePause && m_pauseBlinkOn)) {
            continue;
        }
        painter.setPen(activePause ? activePausePen : pausePen);
        const int y = rowViewportPosition(row) + rowHeight(row) - 1;
        painter.drawLine(0, y, viewport()->width(), y);
    }
}

void WTandaQueueView::prepareTrackMenu(
        WTrackMenu* pTrackMenu, const QModelIndexList& indices) {
    if (!tandaModel()) {
        return;
    }
    const QList<QAction*> actions = pTrackMenu->actions();
    QAction* pBefore = actions.isEmpty() ? nullptr : actions.first();
    if (!pTrackMenu->actions().contains(m_pTandaSeparator)) {
        for (QAction* pAction : std::as_const(m_classifyActions)) {
            pTrackMenu->insertAction(pBefore, pAction);
        }
        pTrackMenu->insertAction(pBefore, m_pTandaSeparator);
    }

    Q_UNUSED(indices);
    const bool canClassify = canClassifySelection();
    for (QAction* pAction : std::as_const(m_classifyActions)) {
        pAction->setVisible(canClassify);
    }
    pTrackMenu->setCortinaToggleAllowed(!selectionContainsTandaLeaves());

    m_pTandaSeparator->setVisible(canClassify);
}

TandaQueueModel* WTandaQueueView::tandaModel() const {
    return qobject_cast<TandaQueueModel*>(model());
}

bool WTandaQueueView::selectedRowsAreHeaders() const {
    TandaQueueModel* pModel = tandaModel();
    if (!pModel || !selectionModel()) {
        return false;
    }
    QSet<int> rows;
    for (const QModelIndex& index : selectionModel()->selectedIndexes()) {
        rows.insert(index.row());
    }
    for (int row : std::as_const(rows)) {
        if (pModel->isHeaderRow(row)) {
            return true;
        }
    }
    return false;
}

QVector<int> WTandaQueueView::selectedQueuePositions(bool* pAllLeaves) const {
    if (pAllLeaves) {
        *pAllLeaves = true;
    }
    QVector<int> positions;
    TandaQueueModel* pModel = tandaModel();
    if (!pModel || !selectionModel()) {
        if (pAllLeaves) {
            *pAllLeaves = false;
        }
        return positions;
    }
    for (const QModelIndex& index : selectionModel()->selectedRows()) {
        const QModelIndex sourceIndex = pModel->mapToSource(index);
        if (!sourceIndex.isValid()) {
            if (pAllLeaves) {
                *pAllLeaves = false;
            }
            continue;
        }
        positions.append(sourceIndex.row() + 1);
    }
    std::sort(positions.begin(), positions.end());
    return positions;
}

bool WTandaQueueView::canClassifySelection() const {
    if (selectedRowsAreHeaders()) {
        return false;
    }
    TandaQueueModel* pModel = tandaModel();
    if (!pModel) {
        return false;
    }
    bool allLeaves = false;
    const QVector<int> positions = selectedQueuePositions(&allLeaves);
    if (!allLeaves || positions.isEmpty()) {
        return false;
    }
    for (int index = 0; index < positions.size(); ++index) {
        const int position = positions.at(index);
        const QModelIndex sourceIndex =
                pModel->playlistModel()->index(position - 1, 0);
        if ((index > 0 && position != positions.at(index - 1) + 1) ||
                m_pAutoDJFeature->tandaQueueState()->spanAtPosition(position) ||
                CortinaRegistry::instance().contains(
                        pModel->playlistModel()->getTrackId(sourceIndex))) {
            return false;
        }
    }
    return true;
}

bool WTandaQueueView::selectionContainsTandaLeaves() const {
    if (selectedRowsAreHeaders()) {
        return true;
    }
    bool allLeaves = false;
    const QVector<int> positions = selectedQueuePositions(&allLeaves);
    if (!allLeaves) {
        return false;
    }
    for (int position : positions) {
        if (m_pAutoDJFeature->tandaQueueState()->spanAtPosition(position)) {
            return true;
        }
    }
    return false;
}

void WTandaQueueView::classifySelection(TandaType type) {
    if (!canClassifySelection()) {
        return;
    }
    QString error;
    if (m_pAutoDJFeature->makeTanda(
                selectedQueuePositions(), type, &error)
                    .isNull() &&
            !error.isEmpty()) {
        showError(error);
    }
}

void WTandaQueueView::setContextTanda(const QUuid& id) {
    m_contextTandaId = id;
    const TandaSpan* pSpan =
            m_pAutoDJFeature->tandaQueueState()->spanById(id);
    const bool collapsed = pSpan && pSpan->collapsed;
    m_pToggleCollapsedAction->setText(
            collapsed ? tr("Expand tanda") : tr("Collapse tanda"));
    TandaQueueModel* pModel = tandaModel();
    m_pRemoveAction->setEnabled(pModel && !pModel->isLocked());
    m_pMoveUpAction->setEnabled(pSpan && pSpan->anchorPosition > 1);
    m_pMoveDownAction->setEnabled(pSpan &&
            pSpan->anchorPosition + pSpan->members.size() - 1 <
                    m_pAutoDJFeature->tandaQueueState()->queueSnapshot().size());
}

void WTandaQueueView::toggleTanda(const QUuid& id) {
    const TandaSpan* pSpan =
            m_pAutoDJFeature->tandaQueueState()->spanById(id);
    if (pSpan) {
        m_pAutoDJFeature->setTandaCollapsed(id, !pSpan->collapsed);
    }
}

int WTandaQueueView::destinationAnchorForDrop(
        const QPoint& position, const QUuid& id) const {
    TandaQueueModel* pModel = tandaModel();
    const TandaSpan* pSpan =
            m_pAutoDJFeature->tandaQueueState()->spanById(id);
    if (!pModel || !pSpan) {
        return -1;
    }
    const int queueSize =
            m_pAutoDJFeature->tandaQueueState()->queueSnapshot().size();
    const int length = pSpan->members.size();
    int row = rowAt(position.y());
    if (row < 0) {
        return queueSize - length + 1;
    }
    const int height = rowHeight(row);
    if (height > 0 && position.y() > rowViewportPosition(row) + height / 2) {
        ++row;
    }
    if (row >= pModel->rowCount()) {
        return queueSize - length + 1;
    }
    if (pModel->isHeaderRow(row)) {
        const TandaSpan* pTarget = m_pAutoDJFeature->tandaQueueState()->spanById(
                pModel->tandaIdForRow(row));
        return pTarget ? pTarget->anchorPosition : -1;
    }
    const int sourceRow = pModel->sourceRowForVisibleRow(row);
    return sourceRow >= 0 ? sourceRow + 1 : -1;
}

void WTandaQueueView::moveContextTanda(bool up) {
    if (m_contextTandaId.isNull()) {
        return;
    }
    QString error;
    const bool moved = up
            ? m_pAutoDJFeature->moveTandaUp(m_contextTandaId, &error)
            : m_pAutoDJFeature->moveTandaDown(m_contextTandaId, &error);
    if (!moved && !error.isEmpty()) {
        showError(error);
    }
}

void WTandaQueueView::showTandaHeaderMenu(
        const QPoint& globalPos, const QUuid& id) {
    if (id.isNull()) {
        return;
    }
    setContextTanda(id);
    QMenu menu(this);
    menu.setObjectName(QStringLiteral("AutoDJContextMenu"));
    menu.addAction(m_pToggleCollapsedAction);
    menu.addMenu(m_pChangeTandaTypeMenu);
    menu.addAction(m_pUngroupAction);
    menu.addAction(m_pRemoveAction);
    menu.addSeparator();
    menu.addAction(m_pMoveUpAction);
    menu.addAction(m_pMoveDownAction);
    menu.exec(globalPos);
}

void WTandaQueueView::showError(const QString& message) {
    QMessageBox::information(this, tr("Auto DJ Queue"), message);
}
