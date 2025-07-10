#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDesktopServices>
#include <QTimer>
#include <QResizeEvent>
#include <QDebug>

#include "phxwidget.h"
#include "phxwebview.h"
#include "../mainwindow.h"
#include "StyleManager.h"

PhxWidget::PhxWidget(QWidget *parent)
    : QWidget(parent)
{

  phxAlive = false;
  consoleVisible = false;
  phxView = new PhxWebView(this);
  QSizePolicy sp_retain = phxView->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  phxView->setSizePolicy(sp_retain);
  phxView->hide();
  mainLayout = new QHBoxLayout(this);
  topRowSubLayout = new QHBoxLayout();
  sizeDownButton = new QPushButton("-");
  sizeUpButton = new QPushButton("+");
  openExternalBrowserButton = new QPushButton(" E ");
  resetBrowserButton = new QPushButton(" R ");
  consoleToggleButton = new QPushButton(" ▲ ");

  QString buttonStyle = StyleManager::guiButton();

  sizeDownButton->setStyleSheet(buttonStyle);
  sizeUpButton->setStyleSheet(buttonStyle);
  openExternalBrowserButton->setStyleSheet(buttonStyle);
  resetBrowserButton->setStyleSheet(buttonStyle);
  consoleToggleButton->setStyleSheet(buttonStyle);

  sizeDownButton->setFixedWidth(30);
  sizeUpButton->setFixedWidth(30);
  openExternalBrowserButton->setFixedWidth(30);
  resetBrowserButton->setFixedWidth(30);
  consoleToggleButton->setFixedWidth(30);

  topRowSubLayout->addWidget(consoleToggleButton);
  topRowSubLayout->addWidget(resetBrowserButton);
  topRowSubLayout->addWidget(openExternalBrowserButton);
  topRowSubLayout->addWidget(sizeDownButton);
  topRowSubLayout->addWidget(sizeUpButton);

  buttonContainer = new QWidget(this);
  buttonContainer->setLayout(topRowSubLayout);
  buttonContainer->setStyleSheet(
    QString("QWidget { "
    "  background-color: %1; "
    "  border-top: 1px solid %2; "
    "  border-bottom: 1px solid %2; "
    "}")
    .arg(StyleManager::Colors::blackAlpha(191))
    .arg(StyleManager::Colors::primaryOrangeAlpha(100))
  );
  buttonContainer->setAttribute(Qt::WA_TranslucentBackground);
  buttonContainer->raise();
  
  mainLayout->addWidget(phxView, 1);
  this->setStyleSheet(QString("PhxWidget { background-color: %1; }")
    .arg(StyleManager::Colors::BLACK));

  connect(sizeDownButton, &QPushButton::released, this, &PhxWidget::handleSizeDown);
  connect(sizeUpButton, &QPushButton::released, this, &PhxWidget::handleSizeUp);
  connect(openExternalBrowserButton, &QPushButton::released, this, &PhxWidget::handleOpenExternalBrowser);
  connect(resetBrowserButton, &QPushButton::released, this, &PhxWidget::handleResetBrowser);
  connect(consoleToggleButton, &QPushButton::released, this, &PhxWidget::handleConsoleToggle);
  connect(phxView, &PhxWebView::loadFinished, this, &PhxWidget::handleLoadFinished);
  QTimer::singleShot(1000, this, SLOT(handleResetBrowser()));
  positionButtonContainer();
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

void PhxWidget::handleConsoleToggle()
{
  emit toggleConsole();
}

void PhxWidget::setConsoleVisible(bool visible)
{
  consoleVisible = visible;
  if (consoleVisible) {
    consoleToggleButton->setText(" ▼ ");
  } else {
    consoleToggleButton->setText(" ▲ ");
  }
}

void PhxWidget::positionButtonContainer()
{
  if (!buttonContainer) return;
  
  QSize containerSize = buttonContainer->sizeHint();
  int margin = 10;
  int scrollbarBuffer = 30;
  
  QWidget* parentWidget = buttonContainer->parentWidget();
  if (!parentWidget) parentWidget = this;
  
  int xPos = parentWidget->width() - containerSize.width() - margin - scrollbarBuffer;
  int yPos = parentWidget->height() - containerSize.height() - margin;
  
  buttonContainer->setGeometry(xPos, yPos, containerSize.width(), containerSize.height());
  buttonContainer->raise();
}

void PhxWidget::raiseButtonContainer()
{
  if (buttonContainer) {
    buttonContainer->raise();
  }
}

void PhxWidget::reparentButtonContainer(QWidget* newParent)
{
  if (buttonContainer && newParent) {
    buttonContainer->setParent(newParent);
    buttonContainer->show();
    buttonContainer->raise();
    positionButtonContainer();
  }
}

void PhxWidget::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  positionButtonContainer();
}

