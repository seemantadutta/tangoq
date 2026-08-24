// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "widget/wdocktitlebar.h"

#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>

WDockTitleBar::WDockTitleBar(QDockWidget* pDock)
        : QWidget(pDock),
          m_pDock(pDock),
          m_pTitle(new QLabel(pDock->windowTitle(), this)),
          m_pFloatButton(new QToolButton(this)),
          m_pCloseButton(new QToolButton(this)),
          m_dragging(false) {
    // A bare QWidget subclass ignores its QSS background unless told to paint a
    // styled background; without this the gradient only "shows through" from the
    // dark dock frame while docked and goes light/native when floating.
    setAttribute(Qt::WA_StyledBackground, true);

    // Object names let the skin QSS target the bar and its buttons (icons and
    // hover colours are set there, so they follow the active theme).
    setObjectName(QStringLiteral("AutoDJDockTitleBar"));
    m_pTitle->setObjectName(QStringLiteral("AutoDJDockTitleLabel"));
    m_pFloatButton->setObjectName(QStringLiteral("AutoDJDockFloatButton"));
    m_pCloseButton->setObjectName(QStringLiteral("AutoDJDockCloseButton"));

    m_pFloatButton->setFocusPolicy(Qt::NoFocus);
    m_pCloseButton->setFocusPolicy(Qt::NoFocus);
    m_pFloatButton->setCursor(Qt::ArrowCursor);
    m_pCloseButton->setCursor(Qt::ArrowCursor);
    m_pFloatButton->setToolTip(tr("Float or dock the Auto DJ queue"));
    m_pCloseButton->setToolTip(tr("Close"));

    auto* pLayout = new QHBoxLayout(this);
    pLayout->setContentsMargins(6, 3, 4, 3);
    pLayout->setSpacing(2);
    pLayout->addWidget(m_pTitle, 1);
    pLayout->addWidget(m_pFloatButton);
    pLayout->addWidget(m_pCloseButton);

    // Toggle between docked and floating. setFloating() restores the dock to its
    // last docked area, which is why we never touch the dock's window flags.
    connect(m_pFloatButton, &QToolButton::clicked, this, [this] {
        m_pDock->setFloating(!m_pDock->isFloating());
    });
    connect(m_pCloseButton, &QToolButton::clicked, m_pDock, &QDockWidget::close);
    connect(m_pDock, &QDockWidget::windowTitleChanged, this, &WDockTitleBar::updateTitle);
}

void WDockTitleBar::updateTitle() {
    m_pTitle->setText(m_pDock->windowTitle());
}

void WDockTitleBar::mousePressEvent(QMouseEvent* pEvent) {
    // Allow dragging the floating window by its themed bar (the native move
    // handle is gone once we replace the title bar).
    if (pEvent->button() == Qt::LeftButton && m_pDock->isFloating()) {
        m_dragging = true;
        m_dragOffset = pEvent->globalPosition().toPoint() -
                m_pDock->frameGeometry().topLeft();
        pEvent->accept();
        return;
    }
    QWidget::mousePressEvent(pEvent);
}

void WDockTitleBar::mouseMoveEvent(QMouseEvent* pEvent) {
    if (m_dragging && (pEvent->buttons() & Qt::LeftButton)) {
        m_pDock->move(pEvent->globalPosition().toPoint() - m_dragOffset);
        pEvent->accept();
        return;
    }
    QWidget::mouseMoveEvent(pEvent);
}

void WDockTitleBar::mouseReleaseEvent(QMouseEvent* pEvent) {
    m_dragging = false;
    QWidget::mouseReleaseEvent(pEvent);
}

void WDockTitleBar::mouseDoubleClickEvent(QMouseEvent* pEvent) {
    if (pEvent->button() == Qt::LeftButton) {
        m_pDock->setFloating(!m_pDock->isFloating());
        pEvent->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(pEvent);
}

#include "moc_wdocktitlebar.cpp"
