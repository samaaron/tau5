#ifndef PHXWEBVIEW_H
#define PHXWEBVIEW_H

#include "sandboxedwebview.h"

class QWebEngineSettings;
class QMenu;
class QContextMenuEvent;

class PhxWebView : public SandboxedWebView
{
  Q_OBJECT

public:
  explicit PhxWebView(bool devMode = false, QWidget *parent = nullptr);
  explicit PhxWebView(bool devMode, bool allowRemoteAccess, QWidget *parent = nullptr);
  
  void setDevToolsAvailable(bool available) { m_devToolsAvailable = available; }

signals:
  void inspectElementRequested();

protected:
  void applyCustomSettings(QWebEngineSettings *settings) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  void showContextMenu(const QPoint &globalPos);
  
  QPoint m_lastContextMenuPos;
  bool m_devToolsAvailable = false;
};

#endif // PHXWEBVIEW_H
