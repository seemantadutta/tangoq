#pragma once

#include <QWidget>

#include "preferences/usersettings.h"
#include "track/track_decl.h"

class WTrackTableView;
class Library;
class PlaylistTableModel;
class QCloseEvent;

// Detached, always-visible Auto DJ queue window. It is a second view onto the same
// Auto DJ PlaylistTableModel as the docked Auto DJ page, so it mirrors the queue
// live - including the now-playing red, the "!!!CORTINA!!!" styling and the
// keep-queue cursor (all model-driven). Toggled from View -> Auto DJ Window.
class DlgAutoDJWindow : public QWidget {
    Q_OBJECT
  public:
    DlgAutoDJWindow(UserSettingsPointer pConfig,
            Library* pLibrary,
            PlaylistTableModel* pModel);
    ~DlgAutoDJWindow() override;

  signals:
    void loadTrack(TrackPointer pTrack);
    void loadTrackToPlayer(TrackPointer pTrack, const QString& group, bool play);
    // Emitted when the user closes the window (so the toggle control can follow).
    void closed();

  protected:
    void closeEvent(QCloseEvent* pEvent) override;

  private:
    WTrackTableView* m_pTrackTableView;
};
