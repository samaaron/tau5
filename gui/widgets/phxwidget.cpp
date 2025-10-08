#include <QHBoxLayout>
#include <QDesktopServices>
#include <QTimer>
#include <QResizeEvent>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QVariant>
#include <QWebChannel>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QFileDialog>
#include <QDateTime>
#include <QPainter>
#include <QDir>
#include <QFileInfo>
#include <cmath>

#include "phxwidget.h"
#include "phxwebview.h"
#include "tau5devbridge.h"
#include "StyleManager.h"
#include "../shared/tau5logger.h"

PhxWidget::PhxWidget(bool devMode, QWidget *parent)
    : PhxWidget(devMode, false, parent)
{
}

PhxWidget::PhxWidget(bool devMode, bool allowRemoteAccess, QWidget *parent)
    : QWidget(parent), m_devMode(devMode), m_allowRemoteAccess(allowRemoteAccess),
      m_webChannel(nullptr), m_devBridge(nullptr)
{
  phxAlive = false;
  retryCount = 0;
  appPageEmitted = false;
  appPageTimer = nullptr;
  phxView = new PhxWebView(m_devMode, allowRemoteAccess, this);
  QSizePolicy sp_retain = phxView->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  phxView->setSizePolicy(sp_retain);
  phxView->hide();
  mainLayout = new QHBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  mainLayout->addWidget(phxView, 1);
  this->setStyleSheet(QString("PhxWidget { background-color: %1; }")
                          .arg(StyleManager::Colors::BLACK));

  // Initialize retry timer
  retryTimer = new QTimer(this);
  retryTimer->setSingleShot(true);
  connect(retryTimer, &QTimer::timeout, this, &PhxWidget::performRetry);

  connect(phxView, &PhxWebView::loadFinished, this, &PhxWidget::handleLoadFinished);

  // Set up web channel for dev mode
  if (m_devMode) {
    setupWebChannel();
  }
}

void PhxWidget::handleSizeDown()
{
  qreal size = phxView->zoomFactor();
  size = size - 0.2;
  if (size < 0.25)
  {
    size = 0.25;
  }
  phxView->setZoomFactor(size);
}

void PhxWidget::handleSizeUp()
{
  qreal size = phxView->zoomFactor();
  size = size + 0.2;
  if (size > 5.0)
  {
    size = 5.0;
  }
  phxView->setZoomFactor(size);
}

void PhxWidget::handleOpenExternalBrowser()
{
  QDesktopServices::openUrl(phxView->url());
}

void PhxWidget::connectToTauPhx(QUrl url)
{
  defaultUrl = url;
  retryCount = 0;
  Tau5Logger::instance().info( QString("[PHX] - connecting to: %1").arg(url.toString()));
  phxView->load(url);

  if (url.toString().contains("/app") && phxAlive) {
    appPageEmitted = false;

    if (appPageTimer) {
      appPageTimer->stop();
      delete appPageTimer;
    }

    // Poll for LiveView to be mounted and rendered before signaling ready
    appPageTimer = new QTimer(this);
    appPageTimer->setSingleShot(false);
    appPageTimer->setInterval(100);  // Check every 100ms
    int pollAttempts = 0;
    connect(appPageTimer, &QTimer::timeout, this, [this, pollAttempts]() mutable {
      pollAttempts++;

      // Check if LiveView is mounted - that's all we need
      QString checkScript = R"(
        (function() {
          // Check if LiveSocket exists and is connected
          if (!window.liveSocket || !window.liveSocket.isConnected()) {
            return 'not_connected';
          }

          // Check if the main LiveView is mounted - that's enough!
          const mainView = document.querySelector('[data-phx-main]');
          if (!mainView || !mainView.__view) {
            return 'not_mounted';
          }

          return 'ready';
        })();
      )";

      phxView->page()->runJavaScript(checkScript, [this, pollAttempts](const QVariant &result) {
        QString status = result.toString();

        if (status == "ready") {
          Tau5Logger::instance().info(QString("[PHX] - app page ready (LiveView mounted after %1ms)").arg(pollAttempts * 100));
          if (!appPageEmitted) {
            emit appPageReady();
            appPageEmitted = true;
          }
          if (appPageTimer) {
            appPageTimer->stop();
            appPageTimer->deleteLater();
            appPageTimer = nullptr;
          }
        } else if (pollAttempts >= 50) {
          // Timeout after 5 seconds - emit anyway
          Tau5Logger::instance().warning(QString("[PHX] - app page timeout after 5s (status: %1)").arg(status));
          if (!appPageEmitted) {
            emit appPageReady();
            appPageEmitted = true;
          }
          if (appPageTimer) {
            appPageTimer->stop();
            appPageTimer->deleteLater();
            appPageTimer = nullptr;
          }
        } else if (pollAttempts % 10 == 0) {
          // Log every second so we can see what's happening
          Tau5Logger::instance().debug(QString("[PHX] - waiting for LiveView... (status: %1, %2ms elapsed)").arg(status).arg(pollAttempts * 100));
        }
      });
    });
    appPageTimer->start();
  }
}

void PhxWidget::handleLoadFinished(bool ok)
{
  if (ok)
  {
    retryCount = 0;
    if (!phxAlive)
    {
      Tau5Logger::instance().info( "[PHX] - initial load finished");
      phxAlive = true;
      phxView->show();
      emit pageLoaded();
    }
  }
  else
  {
    QString currentUrl = phxView->url().toString();
    if (currentUrl.contains("/app")) {
      Tau5Logger::instance().debug(QString("[PHX] Load reported as failed for /app page (expected with LiveView)"));
      return;
    }
    
    if (retryCount < MAX_RETRIES)
    {
      retryCount++;
      
      int delayMs = INITIAL_RETRY_DELAY_MS * std::pow(2, retryCount - 1);
      
      Tau5Logger::instance().warning( 
                  QString("[PHX] - load error, retrying in %1ms (attempt %2/%3)")
                  .arg(delayMs)
                  .arg(retryCount)
                  .arg(MAX_RETRIES));
      
      retryTimer->start(delayMs);
    }
    else
    {
      Tau5Logger::instance().error( 
                  QString("[PHX] - load failed after %1 retries")
                  .arg(MAX_RETRIES));
    }
  }
}

void PhxWidget::handleResetBrowser()
{
  Tau5Logger::instance().info("[PHX] - Hard reset: destroying and recreating web view");

  // Store current state
  QUrl currentUrl = defaultUrl;
  bool devToolsAvailable = false;
  if (phxView) {
    devToolsAvailable = phxView->property("devToolsAvailable").toBool();
  }

  // Disconnect existing connections
  if (phxView) {
    disconnect(phxView, &PhxWebView::loadFinished, this, &PhxWidget::handleLoadFinished);

    // Remove from layout
    mainLayout->removeWidget(phxView);

    // Delete the old view
    phxView->deleteLater();
    phxView = nullptr;
  }

  // Create a new web view
  phxView = new PhxWebView(m_devMode, m_allowRemoteAccess, this);
  QSizePolicy sp_retain = phxView->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  phxView->setSizePolicy(sp_retain);

  // Restore dev tools availability if set
  phxView->setDevToolsAvailable(devToolsAvailable);

  // Add to layout
  mainLayout->addWidget(phxView, 1);

  // Reconnect signals
  connect(phxView, &PhxWebView::loadFinished, this, &PhxWidget::handleLoadFinished);

  // Reset state
  retryCount = 0;
  phxAlive = false;
  appPageEmitted = false;

  // Show the new view
  phxView->show();

  // Recreate web channel if in dev mode
  if (m_devMode) {
    setupWebChannel();
  }

  // Load the URL
  Tau5Logger::instance().info(QString("[PHX] - Loading URL after hard reset: %1").arg(currentUrl.toString()));
  phxView->load(currentUrl);

  // Emit signal that web view has been recreated
  emit webViewRecreated();
}

void PhxWidget::handleSaveAsImage()
{
  if (!phxView) {
    Tau5Logger::instance().warning("[PHX] Cannot save image: no web view available");
    return;
  }

  // Static variable to remember the last directory
  static QString lastSaveDirectory = QDir::homePath();

  // Generate default filename with timestamp
  QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
  QString defaultFileName = QString("tau5_screenshot_%1.png").arg(timestamp);
  QString fullDefaultPath = QDir(lastSaveDirectory).filePath(defaultFileName);

  // Open save dialog
  QString fileName = QFileDialog::getSaveFileName(
    this,
    tr("Save Web View as Image"),
    fullDefaultPath,
    tr("PNG Images (*.png);;JPEG Images (*.jpg *.jpeg);;All Files (*)")
  );

  if (fileName.isEmpty()) {
    return;  // User cancelled
  }

  // Remember the directory for next time
  QFileInfo fileInfo(fileName);
  lastSaveDirectory = fileInfo.absolutePath();

  // Ensure proper extension
  if (!fileName.contains('.')) {
    fileName += ".png";
  }

  // Get the current size of the web view
  QSize webViewSize = phxView->size();

  // Create a pixmap with the web view's size
  QPixmap pixmap(webViewSize);
  pixmap.fill(Qt::white);  // Fill with white background

  // Render the web view onto the pixmap
  QPainter painter(&pixmap);
  phxView->render(&painter);
  painter.end();

  // Save the pixmap
  if (pixmap.save(fileName)) {
    Tau5Logger::instance().info(QString("[PHX] Screenshot saved to: %1").arg(fileName));
  } else {
    Tau5Logger::instance().warning(QString("[PHX] Failed to save screenshot to: %1").arg(fileName));
  }
}

void PhxWidget::setupWebChannel()
{
  if (!phxView || !m_devMode) {
    return;
  }

  Tau5Logger::instance().info("[PHX] Setting up web channel for dev mode");

  // Clean up any existing channel and bridge
  if (m_webChannel) {
    delete m_webChannel;
    m_webChannel = nullptr;
  }
  if (m_devBridge) {
    delete m_devBridge;
    m_devBridge = nullptr;
  }

  // Create new channel and bridge
  m_webChannel = new QWebChannel(this);
  m_devBridge = new Tau5DevBridge(this);

  // Connect the bridge signal to our handler
  connect(m_devBridge, &Tau5DevBridge::hardRefreshRequested, this, &PhxWidget::handleResetBrowser);

  // Register the bridge object with the channel
  m_webChannel->registerObject(QStringLiteral("tau5"), m_devBridge);

  // Set the channel on the page
  phxView->page()->setWebChannel(m_webChannel);

  // Inject the qwebchannel.js library and setup code
  connect(phxView, &PhxWebView::loadFinished, this, [this](bool ok) {
    if (!ok) return;

    // First inject the QWebChannel JavaScript library
    QFile webChannelFile(":/qtwebchannel/qwebchannel.js");
    if (webChannelFile.open(QIODevice::ReadOnly)) {
      QString webChannelJs = QString::fromUtf8(webChannelFile.readAll());
      phxView->page()->runJavaScript(webChannelJs);

      // Then setup our channel
      QString setupScript = R"(
        (function() {
          if (typeof QWebChannel !== 'undefined') {
            new QWebChannel(qt.webChannelTransport, function(channel) {
              window.tau5 = channel.objects.tau5;
              console.log('[Tau5] Web channel connected - tau5.hardRefresh() available');
            });
          } else {
            console.error('[Tau5] QWebChannel not available');
          }
        })();
      )";
      phxView->page()->runJavaScript(setupScript);
    } else {
      Tau5Logger::instance().error("[PHX] Failed to load qwebchannel.js");
    }
  }, Qt::SingleShotConnection);
}

void PhxWidget::performRetry()
{
  Tau5Logger::instance().info( QString("[PHX] - performing retry %1/%2")
              .arg(retryCount)
              .arg(MAX_RETRIES));
  phxView->load(defaultUrl);
}

void PhxWidget::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
}

