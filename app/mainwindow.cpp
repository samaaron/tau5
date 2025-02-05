#include <iostream>
#include <QApplication>
#include "mainwindow.h"
#include "widgets/phxwidget.h"

MainWindow::MainWindow(quint16 port, QWidget *parent)
    : QMainWindow(parent)
{
  QCoreApplication::setOrganizationName("Sonic Pi");

  QUrl phxUrl;
  phxUrl.setUrl("http://localhost");
  phxUrl.setPort(port);
  phxWidget = new PhxWidget(this);
  phxWidget->connectToTauPhx(phxUrl);
  setCentralWidget(phxWidget);
}