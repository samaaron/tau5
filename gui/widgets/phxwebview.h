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
  explicit PhxWebView(QWidget *parent = nullptr);

signals:
  void inspectElementRequested();

protected:
  void applyCustomSettings(QWebEngineSettings *settings) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  void showContextMenu(const QPoint &globalPos);
  
  QPoint m_lastContextMenuPos;
};

#endif // PHXWEBVIEW_H
