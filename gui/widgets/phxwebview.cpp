#include "phxwebview.h"
#include "../styles/StyleManager.h"
#include "debugpane/iconutilities.h"
#include <QWebEngineSettings>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QSvgRenderer>
#include <QPainter>

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
  
  if (m_devToolsAvailable) {
    QAction *inspectAction = contextMenu.addAction(tr("Inspect Element"));
    
    // Create bug icon using IconUtilities
    QString normalColor = StyleManager::Colors::TEXT_PRIMARY;
    QString selectedColor = StyleManager::Colors::ACCENT_PRIMARY;
    
    QIcon bugIcon = IconUtilities::createSvgIcon(
        IconUtilities::Icons::bugSvg(normalColor),
        "",  // No hover state needed for menu
        IconUtilities::Icons::bugSvg(selectedColor));
    
    inspectAction->setIcon(bugIcon);
    
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
