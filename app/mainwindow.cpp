#include <iostream>
#include <QApplication>
#include <QSettings>
#include <QCloseEvent>
#include <QMessageBox>
#include <QMenuBar>
#include "mainwindow.h"
#include "widgets/phxwidget.h"

MainWindow::MainWindow(quint16 port, QWidget *parent)
    : QMainWindow(parent)
{
  QCoreApplication::setOrganizationName("Tau5");
  QCoreApplication::setApplicationName("Tau5");

  resize(1024, 768); // Set to a larger size initially

  // Restore saved window geometry (size and position)
  QSettings settings;
  if (settings.contains("MainWindow/geometry"))
  {
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
  }
  else
  {
    // Optionally log or set a default size if geometry isn't found
    std::cout << "No saved geometry found, using default size." << std::endl;
  }

  QCoreApplication::setOrganizationName("Sonic Pi");

  QUrl phxUrl;
  phxUrl.setUrl("http://localhost");
  phxUrl.setPort(port);
  phxWidget = new PhxWidget(this);
  phxWidget->connectToTauPhx(phxUrl);
  setCentralWidget(phxWidget);
  this->setStyleSheet("background-color: black;");

  // Create the menu bar
  QMenuBar *menuBar = this->menuBar();

  // Add Help menu and About action
  QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
  QAction *aboutAction = helpMenu->addAction(tr("&About"));

  // Connect the About action to the slotb
  connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  // Save window geometry before closing
  QSettings settings;
  settings.setValue("MainWindow/geometry", saveGeometry());

  QMainWindow::closeEvent(event);
}

// Slot to show About dialog
void MainWindow::showAbout()
{
  QMessageBox::about(this, tr("About Tau5"),
                     tr("Sonic Pi Tau5 Tech\n\nby Sam Aaron"));
}