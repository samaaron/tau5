#ifndef PHXWEBVIEW_H
#define PHXWEBVIEW_H

#include "sandboxedwebview.h"

class QWebEngineSettings;

class PhxWebView : public SandboxedWebView
{
  Q_OBJECT

public:
  explicit PhxWebView(QWidget *parent = nullptr);

protected:
  void applyCustomSettings(QWebEngineSettings *settings) override;
};

#endif // PHXWEBVIEW_H
