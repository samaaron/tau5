#include <QHBoxLayout>
#include <QDesktopServices>
#include <QTimer>
#include <QResizeEvent>
#include <QDebug>

#include "phxwidget.h"
#include "phxwebview.h"
#include "StyleManager.h"

PhxWidget::PhxWidget(QWidget *parent)
    : QWidget(parent)
{
  phxAlive = false;
  phxView = new PhxWebView(this);
  QSizePolicy sp_retain = phxView->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  phxView->setSizePolicy(sp_retain);
  phxView->hide();
  mainLayout = new QHBoxLayout(this);
  
  mainLayout->addWidget(phxView, 1);
  this->setStyleSheet(QString("PhxWidget { background-color: %1; }")
    .arg(StyleManager::Colors::BLACK));

  connect(phxView, &PhxWebView::loadFinished, this, &PhxWidget::handleLoadFinished);
  QTimer::singleShot(1000, this, SLOT(handleResetBrowser()));
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
  qDebug() << "[PHX] - connecting to:" << url.toString();
  phxView->load(url);
}

void PhxWidget::handleLoadFinished(bool ok)
{
  if (ok)
  {
    if (!phxAlive)
    {
      qDebug() << "[PHX] - initial load finished";
      phxAlive = true;
      phxView->show();
    }
  }
  else
  {
    qDebug() << "[PHX] - load error";
    phxView->load(defaultUrl);
  }
}

void PhxWidget::handleResetBrowser()
{
  phxView->load(defaultUrl);
}

void PhxWidget::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
}

