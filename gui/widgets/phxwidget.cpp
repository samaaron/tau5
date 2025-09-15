#include <QHBoxLayout>
#include <QDesktopServices>
#include <QTimer>
#include <QResizeEvent>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QVariant>
#include <cmath>

#include "phxwidget.h"
#include "phxwebview.h"
#include "StyleManager.h"
#include "../shared/tau5logger.h"

PhxWidget::PhxWidget(bool devMode, QWidget *parent)
    : PhxWidget(devMode, false, parent)
{
}

PhxWidget::PhxWidget(bool devMode, bool allowRemoteAccess, QWidget *parent)
    : QWidget(parent), m_devMode(devMode)
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
    
    appPageTimer = new QTimer(this);
    appPageTimer->setSingleShot(true);
    connect(appPageTimer, &QTimer::timeout, this, [this]() {
      if (!appPageEmitted) {
        Tau5Logger::instance().info("[PHX] - app page ready (timer-based)");
        emit appPageReady();
        appPageEmitted = true;
      }
      appPageTimer = nullptr;
    });
    appPageTimer->start(1500);
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
  retryCount = 0;
  phxView->load(defaultUrl);
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

