#include <iostream>
#include <QApplication>
#include <QSettings>
#include <QCloseEvent>
#include <QMessageBox>
#include <QMenuBar>
#include "mainwindow.h"
#include "widgets/phxwidget.h"

MainWindow::MainWindow(QWidget *parent)
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

  this->setStyleSheet("background-color: black;");

  // Create the menu bar
  QMenuBar *menuBar = this->menuBar();

  // Add Help menu and About action
  QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
  QAction *aboutAction = helpMenu->addAction(tr("&About"));

  // Connect the About action to the slot
  connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

// Destructor must be defined here where PhxWidget is complete
MainWindow::~MainWindow() = default;

bool MainWindow::connectToServer(quint16 port)
{
  try {
    initializePhxWidget(port);
    return true;
  }
  catch (const std::exception& e) {
    QMessageBox::critical(this, tr("Connection Error"),
                         tr("Failed to initialize connection: %1").arg(e.what()));
    return false;
  }
}

void MainWindow::initializePhxWidget(quint16 port)
{
  QUrl phxUrl;
  phxUrl.setScheme("http");
  phxUrl.setHost("localhost");
  phxUrl.setPort(port);

  phxWidget = std::make_unique<PhxWidget>(this);

  // Assuming connectToTauPhx returns bool indicating success
  // If it doesn't, you may need to adjust this based on actual API
  phxWidget->connectToTauPhx(phxUrl);

  setCentralWidget(phxWidget.get());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  // Save window geometry before closing
  QSettings settings;
  settings.setValue("MainWindow/geometry", saveGeometry());

  QMainWindow::closeEvent(event);
}

// Slot to show About dialog
void MainWindow::showAbout() const
{
  QMessageBox::about(const_cast<MainWindow*>(this), tr("About Tau5"),
                     tr("Sonic Pi Tau5 Tech\n\nby Sam Aaron"));
}