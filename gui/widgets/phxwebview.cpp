#include "phxwebview.h"
#include "../styles/StyleManager.h"
#include <QWebEngineSettings>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QFontDatabase>
#include <QPainter>
#include <QPixmap>

PhxWebView::PhxWebView(bool devMode, QWidget *parent)
    : SandboxedWebView(devMode, parent)
{
  setZoomFactor(1.0);
  setAttribute(Qt::WA_TranslucentBackground);
  setStyleSheet("background:transparent");
  setScrollbarColours(StyleManager::Colors::SCROLLBAR_THUMB, 
                     StyleManager::Colors::BACKGROUND_PRIMARY, 
                     StyleManager::Colors::ACCENT_HIGHLIGHT);
  
  setContextMenuPolicy(Qt::DefaultContextMenu);
}

void PhxWebView::applyCustomSettings(QWebEngineSettings *settings)
{
  settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
}

void PhxWebView::contextMenuEvent(QContextMenuEvent *event)
{
  // Store the position for the inspect action
  m_lastContextMenuPos = event->pos();
  showContextMenu(event->globalPos());
}

void PhxWebView::showContextMenu(const QPoint &globalPos)
{
  QMenu contextMenu(this);
  contextMenu.setStyleSheet(StyleManager::contextMenu());

  // Add standard text editing actions
  QAction *copyAction = page()->action(QWebEnginePage::Copy);
  if (copyAction && copyAction->isEnabled()) {
    contextMenu.addAction(copyAction);
  }

  QAction *cutAction = page()->action(QWebEnginePage::Cut);
  if (cutAction && cutAction->isEnabled()) {
    contextMenu.addAction(cutAction);
  }

  QAction *pasteAction = page()->action(QWebEnginePage::Paste);
  if (pasteAction && pasteAction->isEnabled()) {
    contextMenu.addAction(pasteAction);
  }

  // Add separator before dev tools if we have other items
  if (m_devToolsAvailable && !contextMenu.isEmpty()) {
    contextMenu.addSeparator();
  }

  if (m_devToolsAvailable) {
    QAction *inspectAction = contextMenu.addAction(tr("Inspect Element"));

    QFont codiconFont("codicon");
    codiconFont.setPixelSize(16);

    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setFont(codiconFont);
    painter.setPen(QColor(StyleManager::Colors::TEXT_PRIMARY));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawText(QRect(0, 0, 16, 16), Qt::AlignCenter, QChar(0xEAAF)); // bug icon
    painter.end();

    inspectAction->setIcon(QIcon(pixmap));

    connect(inspectAction, &QAction::triggered, this, [this]() {
      // Qt WebEngine provides direct support for inspecting at a position
      page()->triggerAction(QWebEnginePage::InspectElement);
      emit inspectElementRequested();
    });
  }

  // Only show menu if we have items
  if (!contextMenu.isEmpty()) {
    contextMenu.exec(globalPos);
  }
}
