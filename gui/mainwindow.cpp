#include <iostream>
#include <QApplication>
#include <QSettings>
#include <QCloseEvent>
#include <QMessageBox>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>
#include "mainwindow.h"
#include "widgets/phxwidget.h"
#include "widgets/debugpane.h"
#include "widgets/controllayer.h"
#include "lib/beam.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , beamInstance(nullptr)
{
  QCoreApplication::setOrganizationName("Tau5");
  QCoreApplication::setApplicationName("Tau5");

  resize(1024, 768);

  QSettings settings;
  if (settings.contains("MainWindow/geometry"))
  {
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
  }

  this->setStyleSheet("background-color: black;");

  QMenuBar *menuBar = this->menuBar();
  QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
  QAction *aboutAction = helpMenu->addAction(tr("&About"));
  connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

  initializeDebugPane();
  initializeControlLayer();
}

MainWindow::~MainWindow() = default;

void MainWindow::setBeamInstance(Beam *beam)
{
  beamInstance = beam;
  if (beamInstance && debugPane) {
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
  
  if (debugPane) {
    debugPane->setWebView(phxWidget->getWebView());
    QString dashboardUrl = QString("http://localhost:%1/dev/dashboard").arg(port);
    debugPane->setLiveDashboardUrl(dashboardUrl);
  }
}

void MainWindow::initializeDebugPane()
{
  debugPane = std::make_unique<DebugPane>(this);
  
  int defaultHeight = height() / 2;
  debugPane->resize(width(), defaultHeight);
  debugPane->move(0, height() - defaultHeight);
  debugPane->raise();
  debugPane->hide();
  
  debugPane->restoreSettings();
  
  QSettings settings;
  settings.beginGroup("DebugPane");
  bool shouldBeVisible = settings.value("visible", false).toBool();
  settings.endGroup();
  
  if (shouldBeVisible) {
    QTimer::singleShot(100, [this]() {
      if (debugPane) {
        debugPane->toggle();
      }
    });
  }
  
  connect(debugPane.get(), &DebugPane::visibilityChanged,
          this, [this](bool visible) {
            if (controlLayer) {
              controlLayer->setConsoleVisible(visible);
              controlLayer->raise();
            }
          });
}

void MainWindow::initializeControlLayer()
{
  controlLayer = std::make_unique<ControlLayer>(this);
  controlLayer->raise();
  
  connect(controlLayer.get(), &ControlLayer::sizeDown, this, &MainWindow::handleSizeDown);
  connect(controlLayer.get(), &ControlLayer::sizeUp, this, &MainWindow::handleSizeUp);
  connect(controlLayer.get(), &ControlLayer::openExternalBrowser, this, &MainWindow::handleOpenExternalBrowser);
  connect(controlLayer.get(), &ControlLayer::resetBrowser, this, &MainWindow::handleResetBrowser);
  connect(controlLayer.get(), &ControlLayer::toggleConsole, this, &MainWindow::toggleConsole);
}

void MainWindow::toggleConsole()
{
  if (debugPane) {
    debugPane->toggle();
  }
}

void MainWindow::handleBeamOutput(const QString &output)
{
  if (debugPane) {
    debugPane->appendOutput(output, false);
  }
}

void MainWindow::handleBeamError(const QString &error)
{
  if (debugPane) {
    debugPane->appendOutput(error, true);
  }
}

void MainWindow::handleGuiLog(const QString &message, bool isError)
{
  if (debugPane) {
    debugPane->appendGuiLog(message, isError);
  }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
  QMainWindow::resizeEvent(event);

  if (debugPane && debugPane->isVisible()) {
    debugPane->resize(width(), debugPane->height());
    debugPane->move(0, height() - debugPane->height());
  }
  
  if (controlLayer) {
    controlLayer->positionControls();
    controlLayer->raise();
  }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  QSettings settings;
  settings.setValue("MainWindow/geometry", saveGeometry());
  
  if (debugPane) {
    debugPane->saveSettings();
  }
  
  QMainWindow::closeEvent(event);
}

void MainWindow::showAbout() const
{
  QMessageBox::about(const_cast<MainWindow*>(this), tr("About Tau5"),
                     tr("Sonic Pi Tau5 Tech\n\nby Sam Aaron"));
}

void MainWindow::handleSizeDown()
{
  if (phxWidget) {
    phxWidget->handleSizeDown();
  }
}

void MainWindow::handleSizeUp()
{
  if (phxWidget) {
    phxWidget->handleSizeUp();
  }
}

void MainWindow::handleOpenExternalBrowser()
{
  if (phxWidget) {
    phxWidget->handleOpenExternalBrowser();
  }
}

void MainWindow::handleResetBrowser()
{
  if (phxWidget) {
    phxWidget->handleResetBrowser();
  }
}