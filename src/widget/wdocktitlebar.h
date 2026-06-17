#pragma once

#include <QPoint>
#include <QWidget>

class QDockWidget;
class QLabel;
class QToolButton;
class QMouseEvent;

/// A themed replacement title bar for a QDockWidget.
///
/// While a QDockWidget is floating, Qt6 on Windows draws a native OS title bar
/// that ignores the skin QSS, so the panel no longer matches the docked
/// gradient. Substituting this widget via QDockWidget::setTitleBarWidget() keeps
/// the skin's themed title bar in both the docked and floating states.
///
/// Note: replacing the native title bar disables Qt's drag-to-redock. Re-docking
/// is offered instead through the float/dock toggle button, a double-click on the
/// bar, and the dock's right-click "Dock to Side" menu. This widget deliberately
/// never alters the dock's window flags, because mangling the flags of a floating
/// top-level dock breaks reparenting when it is docked back.
class WDockTitleBar : public QWidget {
    Q_OBJECT
  public:
    explicit WDockTitleBar(QDockWidget* pDock);

  protected:
    void mousePressEvent(QMouseEvent* pEvent) override;
    void mouseMoveEvent(QMouseEvent* pEvent) override;
    void mouseReleaseEvent(QMouseEvent* pEvent) override;
    void mouseDoubleClickEvent(QMouseEvent* pEvent) override;

  private slots:
    // Keeps the label text in sync with the dock's window title.
    void updateTitle();

  private:
    QDockWidget* m_pDock;
    QLabel* m_pTitle;
    QToolButton* m_pFloatButton;
    QToolButton* m_pCloseButton;
    QPoint m_dragOffset;
    bool m_dragging;
};
