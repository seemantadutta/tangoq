#pragma once

#include "library/basesqltablemodel.h"

class LibraryTableModel : public BaseSqlTableModel {
    Q_OBJECT
  public:
    LibraryTableModel(QObject* parent, TrackCollectionManager* pTrackCollectionManager,
                      const char* settingsNamespace);
    ~LibraryTableModel() override;

    void setTableModel();

    bool isColumnInternal(int column) final;
    // TangoQ: a fresh install shows only a handful of columns, in a set order.
    bool isColumnHiddenByDefault(int column) final;
    QList<int> defaultColumnOrder() const final;
    // Takes a list of locations and add the tracks to the library. Returns the
    // number of successful additions.
    int addTracks(const QModelIndex& index, const QList<QString>& locations) final;
    TrackModel::Capabilities getCapabilities() const final;
};
