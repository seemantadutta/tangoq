#pragma once

#include <QLineEdit>

#include "preferences/usersettings.h"
#include "track/track_decl.h"
#include "track/trackid.h"
#include "util/parented_ptr.h"
#include "widget/trackdroptarget.h"
#include "widget/wlabel.h"

class ControlProxy;
class ControlPushButton;
class Library;
class WTrackMenu;

/// Custom editor that allows editing track properties via 'selected click' like
/// in the library table. Commits changed data on Enter/Return key press and on
/// FocusOut events.
class WTrackPropertyEditor : public QLineEdit {
    Q_OBJECT
  public:
    WTrackPropertyEditor(QWidget* pParent);

  protected:
    bool eventFilter(QObject* pObj, QEvent* pEvent);

  signals:
    void commitEditorData(const QString& text);
};

// Label that displays the value of a certain track property.
// If the property is editable the value can be edited inline by first selecting
// the label with single click, then clicking again to open the editor.
// The property name is stored in m_editProperty and m_displayProperty, which are
// identical, except for 'titleInfo' (display value, not writable) which we map
// to 'title' (m_editProperty).
class WTrackProperty : public WLabel, public TrackDropTarget {
    Q_OBJECT
  public:
    WTrackProperty(
            QWidget* pParent,
            UserSettingsPointer pConfig,
            Library* pLibrary,
            const QString& group,
            bool isMainDeck);
    ~WTrackProperty() override;
    // Custom property to allow skins to style the 'selected' state when the
    // widget awaits a second click to open the editor. It's reset automatically
    // if no second click is registered within the specified interval.
    // Usage in css: WTrackProperty[selected="true"/"false"] { /* styles */ }
    Q_PROPERTY(bool selected READ isSelected NOTIFY selectedStateChanged);
    // Set while a cortina-tagged track is loaded and Tango mode is on, so skins
    // can colour the title the way the Auto DJ list does.
    // Usage in css: WTrackProperty[cortina="true"] { /* styles */ }
    Q_PROPERTY(bool cortina READ isCortina NOTIFY cortinaStateChanged);

    bool isSelected() const {
        return m_bSelected;
    }

    bool isCortina() const {
        return m_bCortina;
    }

    void setup(const QDomNode& node, const SkinContext& context) override;

  signals:
    void trackDropped(const QString& filename, const QString& group) override;
    void cloneDeck(const QString& sourceGroup, const QString& targetGroup) override;
    void setAndConfirmTrackMenuControl(bool visible);
    void selectedStateChanged(bool state);
    void cortinaStateChanged(bool state);
    void saveCurrentViewState();
    void restoreCurrentViewState();

  public slots:
    void slotTrackLoaded(TrackPointer pTrack);
    void slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack);
    void slotShowTrackMenuChangeRequest(bool show);

  protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

  private slots:
    void slotTrackChanged(TrackId);
    void resetSelectedState();
    void slotCommitEditorData(const QString& text);

  private:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    void updateLabel();
    /// True when the loaded track should be shown as a cortina: a title-ish
    /// property, Tango mode on, and the track tagged in the cortina registry.
    bool showsCortinaMark() const;
    const QString getPropertyStringFromTrack(QString& property) const;
    void restyleAndRepaint();

    void ensureTrackMenuIsCreated();
    const QString m_group;
    const UserSettingsPointer m_pConfig;
    Library* m_pLibrary;
    const bool m_isMainDeck;
    TrackPointer m_pCurrentTrack;

    QString m_displayProperty;
    QString m_editProperty;
    bool m_propertyIsWritable;
    parented_ptr<QTimer> m_pSelectedClickTimer;
    bool m_bSelected;
    bool m_bCortina;
    parented_ptr<WTrackPropertyEditor> m_pEditor;

    parented_ptr<WTrackMenu> m_pTrackMenu;

    /// Only created for title-ish properties, to refresh the cortina mark when
    /// Tango mode is toggled.
    parented_ptr<ControlProxy> m_pKeepQueue;
};
