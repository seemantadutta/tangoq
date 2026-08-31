// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QAbstractProxyModel>
#include <QColor>
#include <QUuid>
#include <QVector>

#include "library/trackmodel.h"

class PlaylistTableModel;
class AutoDJProcessor;
struct TandaSpan;
class TandaQueueState;
class TandaColorPalette;

/// Tango-only presentation adapter for the flat Auto DJ playlist.
///
/// The source playlist remains the playback and persistence authority. This
/// model only inserts virtual tanda header rows and hides children of collapsed
/// tandas.
class TandaQueueModel final : public QAbstractProxyModel, public TrackModel {
    Q_OBJECT

  public:
    enum DataRole {
        RowKindRole = Qt::UserRole + 100,
        TandaIdRole,
        DisclosureActionRole,
    };
    enum class RowKind {
        Track,
        TandaHeader,
    };

    TandaQueueModel(PlaylistTableModel* pSourceModel,
            TandaQueueState* pState,
            AutoDJProcessor* pProcessor = nullptr,
            QObject* pParent = nullptr,
            TandaColorPalette* pColorPalette = nullptr);

    QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
    QModelIndex index(int row,
            int column,
            const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index,
            const QVariant& value,
            int role = Qt::EditRole) override;
    QVariant headerData(int section,
            Qt::Orientation orientation,
            int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;

    bool isHeaderRow(int proxyRow) const;
    QUuid tandaIdForRow(int proxyRow) const;

    int sourceRowForVisibleRow(int proxyRow) const;
    int disclosureColumn() const;
    int summaryColumn() const;
    QString tandaProgressStatesForRow(int proxyRow) const;
    // This helper shares a name with QAbstractProxyModel::mapSelectionToSource
    // but takes a different argument; keep the base overload visible so it is not
    // hidden (-Woverloaded-virtual).
    using QAbstractProxyModel::mapSelectionToSource;
    QModelIndexList mapSelectionToSource(const QModelIndexList& indices) const;
    PlaylistTableModel* playlistModel() const {
        return m_pPlaylistModel;
    }

    TrackPointer getTrack(const QModelIndex& index) const override;
    TrackPointer getTrackByRef(const TrackRef& trackRef) const override;
    QUrl getTrackUrl(const QModelIndex& index) const override;
    QString getTrackLocation(const QModelIndex& index) const override;
    TrackId getTrackId(const QModelIndex& index) const override;
    CoverInfo getCoverInfo(const QModelIndex& index) const override;
    const QVector<int> getTrackRows(TrackId trackId) const override;
    int getTrackRowByPosition(int position) const override;
    const QList<int> getSelectedPositions(const QModelIndexList& indices) const override;
    void search(const QString& searchText) override;
    const QString currentSearch() const override;
    bool isColumnInternal(int column) override;
    bool isColumnHiddenByDefault(int column) override;
    QList<int> defaultColumnOrder() const override;
    const QList<int>& searchColumns() const override;
    void removeTracks(const QModelIndexList& indices) override;
    void cutTracks(const QModelIndexList& indices) override;
    void copyTracks(const QModelIndexList& indices) const override;
    QList<int> pasteTracks(const QModelIndex& index) override;
    int addTracks(const QModelIndex& index, const QList<QString>& locations) override;
    int addTracksWithTrackIds(const QModelIndex& index,
            const QList<TrackId>& tracks,
            int* pOutInsertionPos) override;
    void moveTrack(const QModelIndex& sourceIndex,
            const QModelIndex& destIndex) override;
    bool isLocked() override;
    QAbstractItemDelegate* delegateForColumn(int column, QObject* pParent) override;
    Capabilities getCapabilities() const override;
    QString getModelSetting(const QString& name) override;
    bool setModelSetting(const QString& name, const QVariant& value) override;
    int defaultSortColumn() const override;
    Qt::SortOrder defaultSortOrder() const override;
    void setDefaultSort(int sortColumn, Qt::SortOrder sortOrder) override;
    bool isColumnSortable(int column) const override;
    SortColumnId sortColumnIdFromColumnIndex(int index) const override;
    int columnIndexFromSortColumnId(SortColumnId sortColumn) const override;
    int fieldIndex(const QString& fieldName) const override;
    void select() override;
    void removeTrackRows(const QSet<TrackId>& trackIds) override;
    void maybeStopModelPopulation() override;
    QString modelKey(bool noSearch) const override;
    bool getRequireConfirmationToHideRemoveTracks() override;
    void setRequireConfirmationToHideRemoveTracks(bool require) override;
    bool updateTrackGenre(Track* pTrack, const QString& genre) const override;
#if defined(__EXTRA_METADATA__)
    bool updateTrackMood(Track* pTrack, const QString& mood) const override;
#endif

    // Rename support for the queue view's "Rename tanda..." action.
    // tandaLabel() is the editable label (custom name, or the auto type label);
    // tandaSummary() is the full "label - N track(s)" string shown collapsed.
    QString tandaLabel(const QUuid& id) const;
    QString tandaSummary(const QUuid& id) const;
    // Applies text edited from the tandaSummary() prefill: strips the count
    // suffix to recover the label, reverting to the auto name if it matches the
    // type label. Returns true if the stored name changed.
    bool renameTandaFromDisplayText(const QUuid& id, const QString& text);

  private slots:
    void rebuild();
    void sourceDataChanged(const QModelIndex& topLeft,
            const QModelIndex& bottomRight,
            const QVector<int>& roles);

  private:
    struct VisibleRow {
        RowKind kind{RowKind::Track};
        int sourceRow{-1};
        QUuid tandaId;
    };

    const VisibleRow* visibleRow(int proxyRow) const;
    // Index of the appended, source-less "Tanda Type" column.
    int tandaTypeColumn() const;
    QModelIndex sourceIndexForInsertion(const QModelIndex& proxyIndex) const;
    QString tandaTypeLabel(const QUuid& id) const;
    // Remaps the header-state key to a tango-specific one so the proxy's column
    // layout stays independent of the wrapped Auto DJ playlist model.
    static QString mapModelSettingName(const QString& name);
    QString tandaProgressStates(const TandaSpan& span) const;

    // Computes the current tanda's state (track count, playing index, ordinal in
    // the set) and pushes it to the processor for the toolbar HUD.
    void publishHudTandaState();
    QString tandaDuration(const QUuid& id) const;

    PlaylistTableModel* const m_pPlaylistModel;
    TandaQueueState* const m_pState;
    AutoDJProcessor* const m_pProcessor;
    TandaColorPalette* const m_pColorPalette;
    QVector<VisibleRow> m_visibleRows;
    QVector<int> m_proxyRowBySourceRow;
};
