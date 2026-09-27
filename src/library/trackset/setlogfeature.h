#pragma once

#include <QDateTime>
#include <QPointer>

#include "library/trackset/baseplaylistfeature.h"
#include "preferences/usersettings.h"

class Library;
class QAction;

class SetlogFeature : public BasePlaylistFeature {
    Q_OBJECT

  public:
    SetlogFeature(Library* pLibrary,
            UserSettingsPointer pConfig);
    virtual ~SetlogFeature();

    QVariant title() override;

    void bindLibraryWidget(WLibrary* libraryWidget,
            KeyboardEventFilter* keyboard) override;
    void activatePlaylist(int playlistId) override;

    /// Creates a TangoQ history session named after its start time, e.g.
    /// "2026-09-26 15:16" (with " #2" if that name is taken). Returns its id,
    /// or kInvalidPlaylistId on failure.
    static int createTangoSessionPlaylist(
            PlaylistDAO* pPlaylistDao, const QDateTime& startTime);

  public slots:
    void onRightClick(const QPoint& globalPos) override;
    void onRightClickChild(const QPoint& globalPos, const QModelIndex& index) override;
    void slotJoinWithPrevious();
    void slotMarkAllTracksPlayed();
    void slotLockAllChildPlaylists();
    void slotUnlockAllChildPlaylists();
    void slotDeletePlaylist() override;
    void slotGetNewPlaylist();
    void activate() override;
    void activateChild(const QModelIndex& index) override;
    // TangoQ history: log a track TangoQ started, opening a session if none
    // is open, and end the session when the queue is reset.
    void slotTangoTrackStarted(TrackPointer pTrack);
    void slotTangoSetReset();

  protected:
    QModelIndex constructChildModel(int selectedId);
    void decorateChild(TreeItem* pChild, int playlistId) override;

  private slots:
    void slotPlayingTrackChanged(TrackPointer currentPlayingTrack);
    void slotPlaylistTableChanged(int playlistId) override;
    void slotPlaylistContentOrLockChanged(const QSet<int>& playlistIds) override;
    void slotPlaylistTableRenamed(int playlistId, const QString& newName) override;
    void slotDeleteAllUnlockedChildPlaylists();

  private:
    void deleteAllUnlockedPlaylistsWithFewerTracks();
    // Marks the track played, updates its play count and appends it to the
    // current session.
    void logPlayedTrack(const TrackPointer& pTrack);
    // TangoQ history replaces the stock logging rule; see slotTangoTrackStarted().
    bool isTangoHistory() const;
    void closeTangoSession();
    void lockOrUnlockAllChildPlaylists(bool lock);
    QString getRootViewHtml() const override;

    std::list<TrackId> m_recentTracks;
    QAction* m_pJoinWithPreviousAction;
    QAction* m_pMarkTracksPlayedAction;
    QAction* m_pStartNewPlaylist;
    QAction* m_pLockAllChildPlaylists;
    QAction* m_pUnlockAllChildPlaylists;
    QAction* m_pDeleteAllChildPlaylists;

    int m_currentPlaylistId;
    int m_yearNodeId;
    Library* m_pLibrary;
    UserSettingsPointer m_pConfig;
};
