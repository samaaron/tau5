#include "devwebview.h"
#include "sandboxedwebview.h"
#include "../styles/StyleManager.h"
#include <QWebEngineSettings>
#include <QWebEnginePage>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFontDatabase>
#include <algorithm>

DevWebView::DevWebView(bool devMode, QWidget *parent)
    : QWidget(parent)
{
  // Create main layout
  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0);

  // Create the actual web view
  m_webView = new SandboxedWebView(devMode, this);
  m_webView->setContextMenuPolicy(Qt::DefaultContextMenu);
  m_webView->setScrollbarColours(StyleManager::Colors::SCROLLBAR_THUMB,
                                 StyleManager::Colors::BACKGROUND_PRIMARY,
                                 StyleManager::Colors::ACCENT_HIGHLIGHT);

  // Add web view to layout
  m_layout->addWidget(m_webView);

  // Setup zoom controls at the bottom
  setupZoomControls();
}

void DevWebView::setupZoomControls()
{
  // Create control bar widget
  QWidget *controlBar = new QWidget(this);
  controlBar->setFixedHeight(24);
  controlBar->setStyleSheet(QString("background-color: %1;").arg(StyleManager::Colors::BACKGROUND_PRIMARY));

  QHBoxLayout *controlLayout = new QHBoxLayout(controlBar);
  controlLayout->setContentsMargins(5, 0, 5, 0);
  controlLayout->setSpacing(0);

  // Create button group with top border (like LogWidget)
  QWidget *buttonGroup = new QWidget(controlBar);
  buttonGroup->setStyleSheet(QString(
      "QWidget {"
      "  border-top: 1px solid %1;"
      "}")
      .arg(StyleManager::Colors::primaryOrangeAlpha(60)));

  QHBoxLayout *buttonGroupLayout = new QHBoxLayout(buttonGroup);
  buttonGroupLayout->setContentsMargins(0, 2, 0, 0);
  buttonGroupLayout->setSpacing(5);

  // Load codicon font if not already loaded
  static bool codiconLoaded = false;
  if (!codiconLoaded) {
    QFontDatabase::addApplicationFont(":/fonts/codicon.ttf");
    codiconLoaded = true;
  }

  // Create zoom out button - using codicon icons like LogWidget
  m_zoomOutButton = new QPushButton(QChar(0xEB3B), buttonGroup); // remove/minus icon
  m_zoomOutButton->setToolTip("Zoom Out");
  m_zoomOutButton->setFixedSize(20, 20);
  m_zoomOutButton->setStyleSheet(QString(
      "QPushButton {"
      "  font-family: 'codicon';"
      "  font-size: 14px;"
      "  font-weight: bold;"
      "  color: %1;"
      "  background: transparent;"
      "  border: none;"
      "  padding: 2px;"
      "}"
      "QPushButton:hover {"
      "  color: white;"
      "  background-color: %2;"
      "  border-radius: 3px;"
      "}")
      .arg(StyleManager::Colors::PRIMARY_ORANGE)
      .arg(StyleManager::Colors::blackAlpha(50)));
  connect(m_zoomOutButton, &QPushButton::clicked, this, &DevWebView::zoomOut);
  buttonGroupLayout->addWidget(m_zoomOutButton);

  // Create zoom in button
  m_zoomInButton = new QPushButton(QChar(0xEA60), buttonGroup); // add/plus icon
  m_zoomInButton->setToolTip("Zoom In");
  m_zoomInButton->setFixedSize(20, 20);
  m_zoomInButton->setStyleSheet(QString(
      "QPushButton {"
      "  font-family: 'codicon';"
      "  font-size: 14px;"
      "  font-weight: bold;"
      "  color: %1;"
      "  background: transparent;"
      "  border: none;"
      "  padding: 2px;"
      "}"
      "QPushButton:hover {"
      "  color: white;"
      "  background-color: %2;"
      "  border-radius: 3px;"
      "}")
      .arg(StyleManager::Colors::PRIMARY_ORANGE)
      .arg(StyleManager::Colors::blackAlpha(50)));
  connect(m_zoomInButton, &QPushButton::clicked, this, &DevWebView::zoomIn);
  buttonGroupLayout->addWidget(m_zoomInButton);

  // Add the button group to the control bar
  controlLayout->addWidget(buttonGroup);

  // Add stretch at the end to push buttons to the left
  controlLayout->addStretch();

  // Add control bar to the bottom
  m_layout->addWidget(controlBar);
}

void DevWebView::zoomIn()
{
  qreal currentZoom = m_webView->zoomFactor();
  qreal newZoom = currentZoom + 0.1;
  // Clamp zoom between 0.5 and 3.0
  newZoom = std::max(0.5, std::min(3.0, newZoom));
  m_webView->setZoomFactor(newZoom);
}

void DevWebView::zoomOut()
{
  qreal currentZoom = m_webView->zoomFactor();
  qreal newZoom = currentZoom - 0.1;
  // Clamp zoom between 0.5 and 3.0
  newZoom = std::max(0.5, std::min(3.0, newZoom));
  m_webView->setZoomFactor(newZoom);
}

QWebEnginePage* DevWebView::page() const
{
  return m_webView ? m_webView->page() : nullptr;
}

void DevWebView::setUrl(const QUrl &url)
{
  if (m_webView) {
    m_webView->setUrl(url);
  }
}

void DevWebView::setFallbackUrl(const QUrl &url)
{
  if (m_webView) {
    m_webView->setFallbackUrl(url);
  }
}

QWebEngineSettings* DevWebView::settings() const
{
  return m_webView ? m_webView->settings() : nullptr;
}

void DevWebView::contextMenuEvent(QContextMenuEvent *event)
{
  // Forward to web view
  if (m_webView) {
    QContextMenuEvent forwardEvent(event->reason(),
                                   m_webView->mapFromGlobal(event->globalPos()),
                                   event->globalPos(),
                                   event->modifiers());
    QApplication::sendEvent(m_webView, &forwardEvent);
  }
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