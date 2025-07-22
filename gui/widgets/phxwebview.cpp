#include "phxwebview.h"
#include "../styles/StyleManager.h"
#include <QWebEngineSettings>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QFontDatabase>
#include <QPainter>
#include <QPixmap>

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
    
    // Create bug icon from codicon font
    QFont codiconFont("codicon");
    codiconFont.setPixelSize(16);
    
    // Create pixmap with codicon bug character
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
