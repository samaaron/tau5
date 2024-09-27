#include <iostream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDesktopServices>
#include <QTimer>

#include "phxwidget.h"
#include "phxwebview.h"

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
  topRowSubLayout = new QVBoxLayout();
  sizeDownButton = new QPushButton("-");
  sizeUpButton = new QPushButton("+");
  openExternalBrowserButton = new QPushButton(" E ");
  resetBrowserButton = new QPushButton(" R ");

  QString buttonStyle = "background-color: rgb(240, 153, 55); color: black; border: 1px solid black;";

  // Apply the style to each button
  sizeDownButton->setStyleSheet(buttonStyle);
  sizeUpButton->setStyleSheet(buttonStyle);
  openExternalBrowserButton->setStyleSheet(buttonStyle);
  resetBrowserButton->setStyleSheet(buttonStyle);

  sizeDownButton->setFixedWidth(30); // Adjust width as needed
  sizeUpButton->setFixedWidth(30);
  openExternalBrowserButton->setFixedWidth(30);
  resetBrowserButton->setFixedWidth(30);

  topRowSubLayout->addStretch(1);
  topRowSubLayout->addWidget(resetBrowserButton, 0, Qt::AlignRight);
  topRowSubLayout->addWidget(openExternalBrowserButton, 0, Qt::AlignRight);
  topRowSubLayout->addWidget(sizeDownButton, 0, Qt::AlignRight);
  topRowSubLayout->addWidget(sizeUpButton, 0, Qt::AlignRight);

  // Create a widget to hold the layout and apply background color
  QWidget *buttonContainer = new QWidget(this);
  buttonContainer->setLayout(topRowSubLayout);
  buttonContainer->setStyleSheet("background-color: black;");

  mainLayout->addWidget(phxView, 1);
  mainLayout->addWidget(buttonContainer);

  // phxView->setScrollbarColours(theme->color("ScrollBar"),
  //                            theme->color("ScrollBarBackground"),
  //                            theme->color("ScrollBarHover"));

  this->setStyleSheet("background-color: black;");

  connect(sizeDownButton, &QPushButton::released, this, &PhxWidget::handleSizeDown);
  connect(sizeUpButton, &QPushButton::released, this, &PhxWidget::handleSizeUp);
  connect(openExternalBrowserButton, &QPushButton::released, this, &PhxWidget::handleOpenExternalBrowser);
  connect(resetBrowserButton, &QPushButton::released, this, &PhxWidget::handleResetBrowser);
  connect(phxView, &PhxWebView::loadFinished, this, &PhxWidget::handleLoadFinished);
  QTimer::singleShot(1000, this, SLOT(handleResetBrowser()));
}

void PhxWidget::handleSizeDown()
{
  // zoom out of webview
  // min zoom is 0.25
  qreal size = phxView->zoomFactor();
  size = size - 0.2;
  if (size < 0.25)
  {
    size = 0.25;
  }

  phxView->setZoomFactor(size);
  // resize button
}

void PhxWidget::handleSizeUp()
{
  // zoom into webview
  // max zoom is 5.0
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
  std::cout << "[PHX] - connecting to: " << url.toString().toStdString() << std::endl;
  phxView->load(url);
}

void PhxWidget::handleLoadFinished(bool ok)
{
  if (ok)
  {
    if (!phxAlive)
    {
      std::cout << "[PHX] - initial load finished" << std::endl;
      phxAlive = true;
      phxView->show();
    }
  }
  else
  {
    std::cout << "[PHX] - load error" << std::endl;
    phxView->load(defaultUrl);
  }
}

void PhxWidget::handleResetBrowser()
{
  phxView->load(defaultUrl);
}
