#include "devwebview.h"
#include "../styles/StyleManager.h"
#include <QWebEngineSettings>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>

DevWebView::DevWebView(bool devMode, QWidget *parent)
    : SandboxedWebView(devMode, parent)
{
  setContextMenuPolicy(Qt::DefaultContextMenu);

  setScrollbarColours(StyleManager::Colors::SCROLLBAR_THUMB,
                     StyleManager::Colors::BACKGROUND_PRIMARY,
                     StyleManager::Colors::ACCENT_HIGHLIGHT);
}

void DevWebView::contextMenuEvent(QContextMenuEvent *event)
{
  showContextMenu(event->globalPos());
}

void DevWebView::showContextMenu(const QPoint &globalPos)
{
  QMenu contextMenu(this);
  contextMenu.setStyleSheet(StyleManager::contextMenu());

  QAction *copyAction = page()->action(QWebEnginePage::Copy);
  if (copyAction && copyAction->isEnabled()) {
    contextMenu.addAction(copyAction);
  }

  QAction *selectAllAction = page()->action(QWebEnginePage::SelectAll);
  if (selectAllAction) {
    if (!contextMenu.isEmpty()) {
      contextMenu.addSeparator();
    }
    contextMenu.addAction(selectAllAction);
  }

  if (!contextMenu.isEmpty()) {
    contextMenu.exec(globalPos);
  }
}