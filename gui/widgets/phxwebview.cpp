#include "phxwebview.h"
#include <QWebEngineSettings>

PhxWebView::PhxWebView(QWidget *parent)
    : SandboxedWebView(parent)
{
  setZoomFactor(1.0);
  setAttribute(Qt::WA_TranslucentBackground);
  setStyleSheet("background:transparent");
  setScrollbarColours("#5e5e5e", "black", "#1e90ff");
}

void PhxWebView::applyCustomSettings(QWebEngineSettings *settings)
{
  settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
}
