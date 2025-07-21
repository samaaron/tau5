#include "phxwebview.h"
#include "../styles/StyleManager.h"
#include <QWebEngineSettings>

PhxWebView::PhxWebView(QWidget *parent)
    : SandboxedWebView(parent)
{
  setZoomFactor(1.0);
  setAttribute(Qt::WA_TranslucentBackground);
  setStyleSheet("background:transparent");
  setScrollbarColours(StyleManager::Colors::SCROLLBAR_THUMB, 
                     StyleManager::Colors::BACKGROUND_PRIMARY, 
                     StyleManager::Colors::ACCENT_HIGHLIGHT);
}

void PhxWebView::applyCustomSettings(QWebEngineSettings *settings)
{
  settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
}
