#include <iostream>
#include <QApplication>
#include <QSettings>
#include <QCloseEvent>
#include <QMessageBox>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QWidget>
#include "mainwindow.h"
#include "widgets/phxwidget.h"
#include "widgets/consolewidget.h"
#include "lib/beam.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , beamInstance(nullptr)
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

  // Initialize console
  initializeConsole();
}

// Destructor must be defined here where PhxWidget is complete
MainWindow::~MainWindow() = default;

void MainWindow::setBeamInstance(Beam *beam)
{
  beamInstance = beam;
  if (beamInstance && consoleWidget) {
    // Connect Beam output signals to console
    connect(beamInstance, &Beam::standardOutput,
            this, &MainWindow::handleBeamOutput);
    connect(beamInstance, &Beam::standardError,
            this, &MainWindow::handleBeamError);
  }
}

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
  phxWidget->connectToTauPhx(phxUrl);
  setCentralWidget(phxWidget.get());
  
  connect(phxWidget.get(), &PhxWidget::toggleConsole, this, &MainWindow::toggleConsole);
  phxWidget->reparentButtonContainer(this);
}

void MainWindow::initializeConsole()
{
  consoleWidget = std::make_unique<ConsoleWidget>(this);
  
  int defaultHeight = height() / 3;
  consoleWidget->resize(width(), defaultHeight);
  consoleWidget->move(0, height() - defaultHeight);
  consoleWidget->raise();
  consoleWidget->hide();
  
  connect(consoleWidget.get(), &ConsoleWidget::visibilityChanged,
          this, [this](bool visible) {
            if (phxWidget) {
              phxWidget->setConsoleVisible(visible);
              phxWidget->raiseButtonContainer();
            }
          });
}

void MainWindow::toggleConsole()
{
  if (consoleWidget) {
    consoleWidget->toggle();
  }
}

void MainWindow::handleBeamOutput(const QString &output)
{
  if (consoleWidget) {
    consoleWidget->appendOutput(output, false);
  }
}

void MainWindow::handleBeamError(const QString &error)
{
  if (consoleWidget) {
    consoleWidget->appendOutput(error, true);
  }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
  QMainWindow::resizeEvent(event);

  if (consoleWidget && consoleWidget->isVisible()) {
    consoleWidget->resize(width(), consoleWidget->height());
    consoleWidget->move(0, height() - consoleWidget->height());
  }
  
  if (phxWidget) {
    phxWidget->positionButtonContainer();
  }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  QSettings settings;
  settings.setValue("MainWindow/geometry", saveGeometry());
  QMainWindow::closeEvent(event);
}

void MainWindow::showAbout() const
{
  QMessageBox::about(const_cast<MainWindow*>(this), tr("About Tau5"),
                     tr("Sonic Pi Tau5 Tech\n\nby Sam Aaron"));
}