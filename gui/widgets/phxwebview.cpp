#include "phxwebview.h"
#include "../styles/StyleManager.h"
#include <QWebEngineSettings>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>

PhxWebView::PhxWebView(QWidget *parent)
    : SandboxedWebView(parent)
{
  setZoomFactor(1.0);
  setAttribute(Qt::WA_TranslucentBackground);
  setStyleSheet("background:transparent");
  setScrollbarColours(StyleManager::Colors::SCROLLBAR_THUMB, 
                     StyleManager::Colors::BACKGROUND_PRIMARY, 
                     StyleManager::Colors::ACCENT_HIGHLIGHT);
  
  // Enable context menu (parent class disables it)
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
  
  QAction *inspectAction = contextMenu.addAction(tr("Inspect Element"));
  inspectAction->setIcon(QIcon::fromTheme("document-properties"));
  
  connect(inspectAction, &QAction::triggered, this, [this]() {
    // Qt WebEngine provides direct support for inspecting at a position
    page()->triggerAction(QWebEnginePage::InspectElement);
    emit inspectElementRequested();
  });
  
  contextMenu.exec(globalPos);
}
