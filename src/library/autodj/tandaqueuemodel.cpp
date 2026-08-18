#include "library/autodj/tandaqueuemodel.h"

#include <QBrush>
#include <QFont>
#include <QMimeData>
#include <QSet>
#include <QSqlDatabase>

#include <algorithm>

#include "library/autodj/autodjprocessor.h"
#include "library/autodj/tandaqueuestate.h"
#include "library/dao/trackschema.h"
#include "library/playlisttablemodel.h"
#include "moc_tandaqueuemodel.cpp"
#include "util/duration.h"

namespace {
// Flow-strip type code for a tanda, matching the HUD/processor codes
// (0=T,1=V,2=M,3=N).
int tandaTypeCode(TandaType type) {
    switch (type) {
    case TandaType::Tango:
        return 0;
    case TandaType::Vals:
        return 1;
    case TandaType::Milonga:
        return 2;
    case TandaType::NuevoAlternative:
        return 3;
    }
    return 0;
}
} // namespace

TandaQueueModel::TandaQueueModel(PlaylistTableModel* pSourceModel,
        TandaQueueState* pState,
        AutoDJProcessor* pProcessor,
        QObject* pParent)
        : QAbstractProxyModel(pParent),
          TrackModel(QSqlDatabase(), "autodj_tanda_queue"),
          m_pPlaylistModel(pSourceModel),
          m_pState(pState),
          m_pProcessor(pProcessor) {
    setSourceModel(pSourceModel);

    connect(m_pState,
            &TandaQueueState::spansChanged,
            this,
            &TandaQueueModel::rebuild);
    // The flow anchor changes the strip without touching spans, so refresh the
    // HUD directly rather than doing a full rebuild.
    connect(m_pState,
            &TandaQueueState::flowAnchorChanged,
            this,
            &TandaQueueModel::publishHudTandaState);
    connect(m_pPlaylistModel,
            &QAbstractItemModel::modelReset,
            this,
            &TandaQueueModel::rebuild);
    connect(m_pPlaylistModel,
            &QAbstractItemModel::layoutChanged,
            this,
            &TandaQueueModel::rebuild);
    connect(m_pPlaylistModel,
            &QAbstractItemModel::rowsInserted,
            this,
            &TandaQueueModel::rebuild);
    connect(m_pPlaylistModel,
            &QAbstractItemModel::rowsRemoved,
            this,
            &TandaQueueModel::rebuild);
    connect(m_pPlaylistModel,
            &QAbstractItemModel::dataChanged,
            this,
            &TandaQueueModel::sourceDataChanged);
    rebuild();
}

QModelIndex TandaQueueModel::mapToSource(const QModelIndex& proxyIndex) const {
    if (!proxyIndex.isValid() || proxyIndex.model() != this) {
        return {};
    }
    const VisibleRow* pRow = visibleRow(proxyIndex.row());
    if (!pRow || pRow->kind != RowKind::Track) {
        return {};
    }
    return m_pPlaylistModel->index(pRow->sourceRow, proxyIndex.column());
}

QModelIndex TandaQueueModel::mapFromSource(const QModelIndex& sourceIndex) const {
    if (!sourceIndex.isValid() || sourceIndex.model() != m_pPlaylistModel ||
            sourceIndex.row() < 0 ||
            sourceIndex.row() >= m_proxyRowBySourceRow.size()) {
        return {};
    }
    const int proxyRow = m_proxyRowBySourceRow.at(sourceIndex.row());
    return proxyRow >= 0 ? index(proxyRow, sourceIndex.column()) : QModelIndex();
}

QModelIndex TandaQueueModel::index(
        int row, int column, const QModelIndex& parent) const {
    if (parent.isValid() || row < 0 || row >= rowCount() || column < 0 ||
            column >= columnCount()) {
        return {};
    }
    return createIndex(row, column);
}

QModelIndex TandaQueueModel::parent(const QModelIndex& child) const {
    Q_UNUSED(child);
    return {};
}

int TandaQueueModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_visibleRows.size();
}

int TandaQueueModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_pPlaylistModel->columnCount();
}

QVariant TandaQueueModel::data(const QModelIndex& proxyIndex, int role) const {
    const VisibleRow* pRow = visibleRow(proxyIndex.row());
    if (!pRow || !proxyIndex.isValid()) {
        return {};
    }
    if (pRow->kind == RowKind::Track) {
        if (role == RowKindRole) {
            return static_cast<int>(RowKind::Track);
        }
        if (role == TandaIdRole) {
            return pRow->tandaId;
        }
        if (role == Qt::DisplayRole && !pRow->tandaId.isNull() &&
                proxyIndex.column() == summaryColumn()) {
            const QVariant value =
                    m_pPlaylistModel->data(mapToSource(proxyIndex), role);
            return value.toString().isEmpty()
                    ? value
                    : QVariant(QStringLiteral("    %1").arg(value.toString()));
        }
        return m_pPlaylistModel->data(mapToSource(proxyIndex), role);
    }

    if (role == RowKindRole) {
        return static_cast<int>(RowKind::TandaHeader);
    }
    if (role == TandaIdRole) {
        return pRow->tandaId;
    }
    const TandaSpan* pSpan = m_pState->spanById(pRow->tandaId);
    if (!pSpan) {
        return {};
    }
    if (role == DisclosureActionRole) {
        return pSpan->collapsed ? tr("Expand tanda") : tr("Collapse tanda");
    }
    if (role == Qt::AccessibleTextRole) {
        return QStringLiteral("%1, %2")
                .arg(tandaSummary(pRow->tandaId),
                        pSpan->collapsed ? tr("collapsed") : tr("expanded"));
    }
    if (role == Qt::ToolTipRole) {
        return QStringLiteral("%1\n%2")
                .arg(tandaSummary(pRow->tandaId),
                        pSpan->collapsed ? tr("Click to expand") : tr("Click to collapse"));
    }
    if (role == Qt::FontRole) {
        QFont font;
        font.setBold(true);
        return font;
    }
    if (role == Qt::BackgroundRole) {
        return QBrush(QColor(52, 61, 72));
    }
    const bool active = isActiveTanda(pRow->tandaId);
    if (role == Qt::ForegroundRole) {
        if (active) {
            return QBrush(QColor(0xee, 0x44, 0x44));
        }
        return QBrush(QColor(225, 230, 236));
    }
    if (role == Qt::TextAlignmentRole) {
        return proxyIndex.column() == disclosureColumn()
                ? QVariant::fromValue(Qt::AlignCenter)
                : QVariant::fromValue(Qt::AlignVCenter | Qt::AlignLeft);
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (proxyIndex.column() == summaryColumn()) {
        return QStringLiteral("      %1").arg(tandaSummary(pRow->tandaId));
    }
    if (proxyIndex.column() == m_pPlaylistModel->fieldIndex(LIBRARYTABLE_DURATION)) {
        return tandaDuration(pRow->tandaId);
    }
    return {};
}

bool TandaQueueModel::setData(
        const QModelIndex& proxyIndex, const QVariant& value, int role) {
    const QModelIndex sourceIndex = mapToSource(proxyIndex);
    return sourceIndex.isValid() && m_pPlaylistModel->setData(sourceIndex, value, role);
}

QVariant TandaQueueModel::headerData(
        int section, Qt::Orientation orientation, int role) const {
    return m_pPlaylistModel->headerData(section, orientation, role);
}

Qt::ItemFlags TandaQueueModel::flags(const QModelIndex& proxyIndex) const {
    const QModelIndex sourceIndex = mapToSource(proxyIndex);
    if (sourceIndex.isValid()) {
        return m_pPlaylistModel->flags(sourceIndex);
    }
    return proxyIndex.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                                : Qt::NoItemFlags;
}

QMimeData* TandaQueueModel::mimeData(const QModelIndexList& indexes) const {
    return m_pPlaylistModel->mimeData(mapSelectionToSource(indexes));
}

bool TandaQueueModel::isHeaderRow(int proxyRow) const {
    const VisibleRow* pRow = visibleRow(proxyRow);
    return pRow && pRow->kind == RowKind::TandaHeader;
}

QUuid TandaQueueModel::tandaIdForRow(int proxyRow) const {
    const VisibleRow* pRow = visibleRow(proxyRow);
    return pRow ? pRow->tandaId : QUuid();
}

int TandaQueueModel::sourceRowForVisibleRow(int proxyRow) const {
    const VisibleRow* pRow = visibleRow(proxyRow);
    return pRow && pRow->kind == RowKind::Track ? pRow->sourceRow : -1;
}

int TandaQueueModel::disclosureColumn() const {
    const int positionColumn =
            m_pPlaylistModel->fieldIndex(PLAYLISTTRACKSTABLE_POSITION);
    return positionColumn >= 0 ? positionColumn : 0;
}

int TandaQueueModel::summaryColumn() const {
    const int titleColumn = m_pPlaylistModel->fieldIndex(LIBRARYTABLE_TITLE);
    if (titleColumn >= 0) {
        return titleColumn;
    }
    for (int column = 0; column < columnCount(); ++column) {
        if (column != disclosureColumn() &&
                !m_pPlaylistModel->isColumnInternal(column)) {
            return column;
        }
    }
    return disclosureColumn();
}

QString TandaQueueModel::tandaProgressStatesForRow(int proxyRow) const {
    const VisibleRow* pRow = visibleRow(proxyRow);
    if (!pRow || pRow->kind != RowKind::TandaHeader) {
        return {};
    }
    const TandaSpan* pSpan = m_pState->spanById(pRow->tandaId);
    return pSpan ? tandaProgressStates(*pSpan) : QString();
}

QModelIndexList TandaQueueModel::mapSelectionToSource(
        const QModelIndexList& indices) const {
    QSet<int> sourceRows;
    for (const QModelIndex& index : indices) {
        const VisibleRow* pRow = visibleRow(index.row());
        if (!pRow || !index.isValid()) {
            continue;
        }
        if (pRow->kind == RowKind::Track) {
            if (pRow->sourceRow >= 0) {
                sourceRows.insert(pRow->sourceRow);
            }
            continue;
        }
        const TandaSpan* pSpan = m_pState->spanById(pRow->tandaId);
        if (!pSpan) {
            continue;
        }
        for (int offset = 0; offset < pSpan->members.size(); ++offset) {
            const int sourceRow = pSpan->anchorPosition - 1 + offset;
            if (sourceRow >= 0 && sourceRow < m_pPlaylistModel->rowCount()) {
                sourceRows.insert(sourceRow);
            }
        }
    }
    QVector<int> sortedRows;
    sortedRows.reserve(sourceRows.size());
    for (int sourceRow : sourceRows) {
        sortedRows.append(sourceRow);
    }
    std::sort(sortedRows.begin(), sortedRows.end());

    QModelIndexList sourceIndices;
    sourceIndices.reserve(sortedRows.size());
    for (int sourceRow : sortedRows) {
        sourceIndices.append(m_pPlaylistModel->index(sourceRow, 0));
    }
    return sourceIndices;
}

TrackPointer TandaQueueModel::getTrack(const QModelIndex& index) const {
    return m_pPlaylistModel->getTrack(mapToSource(index));
}

TrackPointer TandaQueueModel::getTrackByRef(const TrackRef& trackRef) const {
    return m_pPlaylistModel->getTrackByRef(trackRef);
}

QUrl TandaQueueModel::getTrackUrl(const QModelIndex& index) const {
    return m_pPlaylistModel->getTrackUrl(mapToSource(index));
}

QString TandaQueueModel::getTrackLocation(const QModelIndex& index) const {
    return m_pPlaylistModel->getTrackLocation(mapToSource(index));
}

TrackId TandaQueueModel::getTrackId(const QModelIndex& index) const {
    return m_pPlaylistModel->getTrackId(mapToSource(index));
}

CoverInfo TandaQueueModel::getCoverInfo(const QModelIndex& index) const {
    return m_pPlaylistModel->getCoverInfo(mapToSource(index));
}

const QVector<int> TandaQueueModel::getTrackRows(TrackId trackId) const {
    QVector<int> proxyRows;
    for (int sourceRow : m_pPlaylistModel->getTrackRows(trackId)) {
        if (sourceRow >= 0 && sourceRow < m_proxyRowBySourceRow.size()) {
            const int proxyRow = m_proxyRowBySourceRow.at(sourceRow);
            if (proxyRow >= 0) {
                proxyRows.append(proxyRow);
            }
        }
    }
    return proxyRows;
}

int TandaQueueModel::getTrackRowByPosition(int position) const {
    const int sourceRow = m_pPlaylistModel->getTrackRowByPosition(position);
    return sourceRow >= 0 && sourceRow < m_proxyRowBySourceRow.size()
            ? m_proxyRowBySourceRow.at(sourceRow)
            : -1;
}

const QList<int> TandaQueueModel::getSelectedPositions(
        const QModelIndexList& indices) const {
    return m_pPlaylistModel->getSelectedPositions(mapSelectionToSource(indices));
}

void TandaQueueModel::search(const QString& searchText) {
    m_pPlaylistModel->search(searchText);
}

const QString TandaQueueModel::currentSearch() const {
    return m_pPlaylistModel->currentSearch();
}

bool TandaQueueModel::isColumnInternal(int column) {
    return m_pPlaylistModel->isColumnInternal(column);
}

bool TandaQueueModel::isColumnHiddenByDefault(int column) {
    return m_pPlaylistModel->isColumnHiddenByDefault(column);
}

const QList<int>& TandaQueueModel::searchColumns() const {
    return m_pPlaylistModel->searchColumns();
}

void TandaQueueModel::removeTracks(const QModelIndexList& indices) {
    m_pPlaylistModel->removeTracks(mapSelectionToSource(indices));
}

void TandaQueueModel::cutTracks(const QModelIndexList& indices) {
    m_pPlaylistModel->cutTracks(mapSelectionToSource(indices));
}

void TandaQueueModel::copyTracks(const QModelIndexList& indices) const {
    m_pPlaylistModel->copyTracks(mapSelectionToSource(indices));
}

QList<int> TandaQueueModel::pasteTracks(const QModelIndex& index) {
    const QList<int> sourceRows =
            m_pPlaylistModel->pasteTracks(sourceIndexForInsertion(index));
    QList<int> proxyRows;
    for (int sourceRow : sourceRows) {
        if (sourceRow >= 0 && sourceRow < m_proxyRowBySourceRow.size()) {
            const int proxyRow = m_proxyRowBySourceRow.at(sourceRow);
            if (proxyRow >= 0) {
                proxyRows.append(proxyRow);
            }
        }
    }
    return proxyRows;
}

int TandaQueueModel::addTracks(
        const QModelIndex& index, const QList<QString>& locations) {
    return m_pPlaylistModel->addTracks(sourceIndexForInsertion(index), locations);
}

int TandaQueueModel::addTracksWithTrackIds(const QModelIndex& index,
        const QList<TrackId>& tracks,
        int* pOutInsertionPos) {
    return m_pPlaylistModel->addTracksWithTrackIds(
            sourceIndexForInsertion(index), tracks, pOutInsertionPos);
}

void TandaQueueModel::moveTrack(
        const QModelIndex& sourceIndex, const QModelIndex& destIndex) {
    const QModelIndex mappedSource = mapToSource(sourceIndex);
    if (!mappedSource.isValid()) {
        return;
    }
    m_pPlaylistModel->moveTrack(
            mappedSource, sourceIndexForInsertion(destIndex));
}

bool TandaQueueModel::isLocked() {
    return m_pPlaylistModel->isLocked();
}

QAbstractItemDelegate* TandaQueueModel::delegateForColumn(
        int column, QObject* pParent) {
    return m_pPlaylistModel->delegateForColumn(column, pParent);
}

TrackModel::Capabilities TandaQueueModel::getCapabilities() const {
    return m_pPlaylistModel->getCapabilities();
}

QString TandaQueueModel::getModelSetting(const QString& name) {
    return m_pPlaylistModel->getModelSetting(name);
}

bool TandaQueueModel::setModelSetting(
        const QString& name, const QVariant& value) {
    return m_pPlaylistModel->setModelSetting(name, value);
}

int TandaQueueModel::defaultSortColumn() const {
    return m_pPlaylistModel->defaultSortColumn();
}

Qt::SortOrder TandaQueueModel::defaultSortOrder() const {
    return m_pPlaylistModel->defaultSortOrder();
}

void TandaQueueModel::setDefaultSort(
        int sortColumn, Qt::SortOrder sortOrder) {
    m_pPlaylistModel->setDefaultSort(sortColumn, sortOrder);
}

bool TandaQueueModel::isColumnSortable(int column) const {
    return m_pPlaylistModel->isColumnSortable(column);
}

TrackModel::SortColumnId TandaQueueModel::sortColumnIdFromColumnIndex(
        int index) const {
    return m_pPlaylistModel->sortColumnIdFromColumnIndex(index);
}

int TandaQueueModel::columnIndexFromSortColumnId(
        SortColumnId sortColumn) const {
    return m_pPlaylistModel->columnIndexFromSortColumnId(sortColumn);
}

int TandaQueueModel::fieldIndex(const QString& fieldName) const {
    return m_pPlaylistModel->fieldIndex(fieldName);
}

void TandaQueueModel::select() {
    m_pPlaylistModel->select();
}

void TandaQueueModel::removeTrackRows(const QSet<TrackId>& trackIds) {
    m_pPlaylistModel->removeTrackRows(trackIds);
}

void TandaQueueModel::maybeStopModelPopulation() {
    m_pPlaylistModel->maybeStopModelPopulation();
}

QString TandaQueueModel::modelKey(bool noSearch) const {
    return m_pPlaylistModel->modelKey(noSearch) + QStringLiteral(":tandas");
}

bool TandaQueueModel::getRequireConfirmationToHideRemoveTracks() {
    return m_pPlaylistModel->getRequireConfirmationToHideRemoveTracks();
}

void TandaQueueModel::setRequireConfirmationToHideRemoveTracks(bool require) {
    m_pPlaylistModel->setRequireConfirmationToHideRemoveTracks(require);
}

bool TandaQueueModel::updateTrackGenre(
        Track* pTrack, const QString& genre) const {
    return m_pPlaylistModel->updateTrackGenre(pTrack, genre);
}

#if defined(__EXTRA_METADATA__)
bool TandaQueueModel::updateTrackMood(
        Track* pTrack, const QString& mood) const {
    return m_pPlaylistModel->updateTrackMood(pTrack, mood);
}
#endif

void TandaQueueModel::rebuild() {
    beginResetModel();
    m_visibleRows.clear();
    m_proxyRowBySourceRow.fill(-1, m_pPlaylistModel->rowCount());

    int sourceRow = 0;
    while (sourceRow < m_pPlaylistModel->rowCount()) {
        const int position = sourceRow + 1;
        const TandaSpan* pSpan = m_pState->spanAtPosition(position);
        if (pSpan && pSpan->anchorPosition == position) {
            m_visibleRows.append(
                    {RowKind::TandaHeader, -1, pSpan->id});
            if (!pSpan->collapsed) {
                for (int offset = 0;
                        offset < pSpan->members.size() &&
                        sourceRow + offset < m_pPlaylistModel->rowCount();
                        ++offset) {
                    m_proxyRowBySourceRow[sourceRow + offset] =
                            m_visibleRows.size();
                    m_visibleRows.append(
                            {RowKind::Track, sourceRow + offset, pSpan->id});
                }
            }
            sourceRow += pSpan->members.size();
            continue;
        }
        m_proxyRowBySourceRow[sourceRow] = m_visibleRows.size();
        m_visibleRows.append({RowKind::Track, sourceRow, {}});
        ++sourceRow;
    }
    endResetModel();
    publishHudTandaState();
}

void TandaQueueModel::publishHudTandaState() {
    if (!m_pProcessor) {
        return;
    }
    const int activePosition = m_pProcessor->activeKeepQueuePosition(); // 1-based
    int trackCount = 0;
    int playingIndex = -1; // -1 also means "preview" (dimmed) to the HUD
    int flowIndex = -1;
    // Walk the tanda spans in order once. Always collect each tanda's type by
    // ordinal (the flow overlay needs it even while Auto DJ is stopped, so the
    // strip and "!" guide list-building). When a set is active, also find the
    // span containing the active position (a tanda is playing), else the first
    // span after it (a cortina or loose track is active - preview the upcoming
    // tanda dimmed).
    QVector<int> tandaTypes;
    const TandaSpan* pActive = nullptr;
    const TandaSpan* pUpcoming = nullptr;
    int activeOrdinal = -1;
    int upcomingOrdinal = -1;
    // Ordinal of the session flow anchor's tanda, so the processor can rebase.
    const QUuid anchorId = m_pState->flowAnchorTandaId();
    int anchorOrdinal = -1;
    int ordinal = 0;
    int pos = 1;
    const int rowCount = m_pPlaylistModel->rowCount();
    while (pos <= rowCount) {
        const TandaSpan* pSpan = m_pState->spanAtPosition(pos);
        if (pSpan && pSpan->anchorPosition == pos) {
            tandaTypes.append(tandaTypeCode(pSpan->type));
            if (!anchorId.isNull() && pSpan->id == anchorId) {
                anchorOrdinal = ordinal;
            }
            const int spanEnd = pSpan->anchorPosition + pSpan->members.size();
            if (activePosition > 0 &&
                    activePosition >= pSpan->anchorPosition &&
                    activePosition < spanEnd) {
                pActive = pSpan;
                activeOrdinal = ordinal;
            } else if (activePosition > 0 && !pUpcoming &&
                    pSpan->anchorPosition > activePosition) {
                pUpcoming = pSpan;
                upcomingOrdinal = ordinal;
            }
            ++ordinal;
            pos += pSpan->members.size();
        } else {
            ++pos;
        }
    }
    if (pActive) {
        trackCount = pActive->members.size();
        playingIndex = activePosition - pActive->anchorPosition; // 0-based
        flowIndex = activeOrdinal;
    } else if (pUpcoming) {
        // Preview the coming tanda: full track count, nothing playing yet
        // (playingIndex -1 -> dimmed), flow highlight advanced to it.
        trackCount = pUpcoming->members.size();
        playingIndex = -1;
        flowIndex = upcomingOrdinal;
    }
    // offset = anchorSlot - anchorOrdinal; 0 when there is no anchor or its tanda
    // is gone (the anchor is then simply ignored, per most-recent-wins).
    const int anchorSlot = m_pState->flowAnchorSlot();
    const int flowOffset =
            (anchorSlot >= 0 && anchorOrdinal >= 0) ? anchorSlot - anchorOrdinal : 0;
    m_pProcessor->setHudTandaState(
            trackCount, playingIndex, flowIndex, tandaTypes, flowOffset);
}

void TandaQueueModel::setFlowAnchor(const QUuid& tandaId, int slot) {
    if (m_pState) {
        m_pState->setFlowAnchor(tandaId, slot);
    }
}

QVector<int> TandaQueueModel::flowPatternTypes() const {
    return m_pProcessor ? m_pProcessor->hudFlowPatternTypes() : QVector<int>();
}

int TandaQueueModel::ordinalForTanda(const QUuid& tandaId) const {
    if (tandaId.isNull()) {
        return -1;
    }
    int ordinal = 0;
    int pos = 1;
    const int rowCount = m_pPlaylistModel->rowCount();
    while (pos <= rowCount) {
        const TandaSpan* pSpan = m_pState->spanAtPosition(pos);
        if (pSpan && pSpan->anchorPosition == pos) {
            if (pSpan->id == tandaId) {
                return ordinal;
            }
            ++ordinal;
            pos += pSpan->members.size();
        } else {
            ++pos;
        }
    }
    return -1;
}

int TandaQueueModel::currentFlowSlot(const QUuid& tandaId) const {
    if (!m_pProcessor) {
        return -1;
    }
    const int len = m_pProcessor->hudFlowPatternLength();
    const int ordinal = ordinalForTanda(tandaId);
    if (len <= 0 || ordinal < 0) {
        return -1;
    }
    // Same rebasing as publishHudFlow: slot = (ordinal + offset) mod len.
    const int anchorSlot = m_pState->flowAnchorSlot();
    const int anchorOrdinal = ordinalForTanda(m_pState->flowAnchorTandaId());
    const int offset =
            (anchorSlot >= 0 && anchorOrdinal >= 0) ? anchorSlot - anchorOrdinal : 0;
    const int r = (ordinal + offset) % len;
    return r < 0 ? r + len : r;
}

void TandaQueueModel::sourceDataChanged(const QModelIndex& topLeft,
        const QModelIndex& bottomRight,
        const QVector<int>& roles) {
    QSet<QUuid> changedTandas;
    for (int sourceRow = topLeft.row(); sourceRow <= bottomRight.row(); ++sourceRow) {
        if (const TandaSpan* pSpan = m_pState->spanAtPosition(sourceRow + 1)) {
            changedTandas.insert(pSpan->id);
        }
        if (sourceRow < 0 || sourceRow >= m_proxyRowBySourceRow.size()) {
            continue;
        }
        const int proxyRow = m_proxyRowBySourceRow.at(sourceRow);
        if (proxyRow >= 0) {
            const VisibleRow* pRow = visibleRow(proxyRow);
            if (pRow && !pRow->tandaId.isNull()) {
                changedTandas.insert(pRow->tandaId);
            }
            emit dataChanged(index(proxyRow, topLeft.column()),
                    index(proxyRow, bottomRight.column()),
                    roles);
        }
    }
    for (int proxyRow = 0; proxyRow < m_visibleRows.size(); ++proxyRow) {
        const VisibleRow& row = m_visibleRows.at(proxyRow);
        if (row.kind == RowKind::TandaHeader &&
                changedTandas.contains(row.tandaId)) {
            emit dataChanged(index(proxyRow, 0),
                    index(proxyRow, columnCount() - 1));
        }
    }
    // The active track (and thus the pip/flow state) can advance without a
    // structural rebuild, so refresh the HUD here too.
    publishHudTandaState();
}

const TandaQueueModel::VisibleRow* TandaQueueModel::visibleRow(
        int proxyRow) const {
    return proxyRow >= 0 && proxyRow < m_visibleRows.size()
            ? &m_visibleRows.at(proxyRow)
            : nullptr;
}

QModelIndex TandaQueueModel::sourceIndexForInsertion(
        const QModelIndex& proxyIndex) const {
    const QModelIndex mapped = mapToSource(proxyIndex);
    if (mapped.isValid()) {
        return mapped;
    }
    const VisibleRow* pRow = visibleRow(proxyIndex.row());
    if (pRow && pRow->kind == RowKind::TandaHeader) {
        const TandaSpan* pSpan = m_pState->spanById(pRow->tandaId);
        if (pSpan) {
            return m_pPlaylistModel->index(
                    pSpan->anchorPosition - 1, proxyIndex.column());
        }
    }
    return {};
}

bool TandaQueueModel::isActiveTanda(const QUuid& id) const {
    const TandaSpan* pSpan = m_pState->spanById(id);
    if (!pSpan) {
        return false;
    }
    if (m_pProcessor) {
        const int activePosition = m_pProcessor->activeKeepQueuePosition();
        if (activePosition > 0) {
            return activePosition >= pSpan->anchorPosition &&
                    activePosition < pSpan->anchorPosition + pSpan->members.size();
        }
    }
    const QColor activeColor(0xee, 0x44, 0x44);
    for (int offset = 0; offset < pSpan->members.size(); ++offset) {
        const int sourceRow = pSpan->anchorPosition - 1 + offset;
        if (sourceRow < 0 || sourceRow >= m_pPlaylistModel->rowCount()) {
            continue;
        }
        const QVariant value = m_pPlaylistModel->data(
                m_pPlaylistModel->index(sourceRow, 0), Qt::ForegroundRole);
        if (value.canConvert<QColor>() &&
                value.value<QColor>() == activeColor) {
            return true;
        }
        if (value.canConvert<QBrush>() &&
                value.value<QBrush>().color() == activeColor) {
            return true;
        }
    }
    return false;
}

QString TandaQueueModel::tandaTypeLabel(const QUuid& id) const {
    const TandaSpan* pSpan = m_pState->spanById(id);
    if (!pSpan) {
        return {};
    }
    switch (pSpan->type) {
    case TandaType::Tango:
        return tr("Tango tanda");
    case TandaType::Vals:
        return tr("Vals tanda");
    case TandaType::Milonga:
        return tr("Milonga tanda");
    case TandaType::NuevoAlternative:
        return tr("Nuevo / Alternative tanda");
    }
    return tr("Tanda");
}

QString TandaQueueModel::tandaSummary(const QUuid& id) const {
    const TandaSpan* pSpan = m_pState->spanById(id);
    if (!pSpan) {
        return {};
    }
    QString label = pSpan->name.isEmpty() ? tandaTypeLabel(id)
                                          : pSpan->name;
    return tr("%1 — %n track(s)", nullptr, pSpan->members.size()).arg(label);
}

QString TandaQueueModel::tandaProgressStates(const TandaSpan& span) const {
    if (!m_pProcessor || span.members.isEmpty()) {
        return {};
    }
    const int activePosition = m_pProcessor->activeKeepQueuePosition();
    QString states;
    states.reserve(span.members.size());
    for (int offset = 0; offset < span.members.size(); ++offset) {
        const int position = span.anchorPosition + offset;
        if (activePosition <= 0 || position > activePosition) {
            states.append(QLatin1Char('0')); // unplayed
        } else if (position == activePosition) {
            states.append(QLatin1Char('h')); // currently playing
        } else {
            states.append(QLatin1Char('1')); // played
        }
    }
    return states;
}

QString TandaQueueModel::tandaDuration(const QUuid& id) const {
    const TandaSpan* pSpan = m_pState->spanById(id);
    if (!pSpan) {
        return {};
    }
    double seconds = 0.0;
    for (int offset = 0; offset < pSpan->members.size(); ++offset) {
        seconds += m_pPlaylistModel->durationSecondsForRow(
                pSpan->anchorPosition - 1 + offset);
    }
    return mixxx::DurationBase::formatTime(seconds);
}
