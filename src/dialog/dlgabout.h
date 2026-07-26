#pragma once

#include <QDialog>

#include "dialog/ui_dlgaboutdlg.h"

class QEvent;
class QObject;

class DlgAbout : public QDialog, public Ui::DlgAboutDlg {
    Q_OBJECT
  public:
    DlgAbout();

  protected:
    /// Makes the wordmark clickable. QSvgWidget has no clicked signal, so the
    /// click is caught here rather than by subclassing it just for this.
    bool eventFilter(QObject* pObject, QEvent* pEvent) override;
};
