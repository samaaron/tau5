#ifndef DEVWEBVIEW_H
#define DEVWEBVIEW_H

#include "sandboxedwebview.h"

class QMenu;
class QContextMenuEvent;

class DevWebView : public SandboxedWebView
{
  Q_OBJECT

public:
  explicit DevWebView(bool devMode = false, QWidget *parent = nullptr);

protected:
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  void showContextMenu(const QPoint &globalPos);
};

#endif // DEVWEBVIEW_H