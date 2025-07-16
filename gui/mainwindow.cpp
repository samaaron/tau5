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
#include "logger.h"
MainWindow::MainWindow(bool devMode, QWidget *parent)
    : QMainWindow(parent)
    , beamInstance(nullptr)
    , m_devMode(devMode)
    , m_mainWindowLoaded(false)
    , m_liveDashboardLoaded(false)
    , m_elixirConsoleLoaded(false)
    , m_webDevToolsLoaded(false)
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

  connect(this, &MainWindow::allComponentsLoaded, []() {
    Logger::log(Logger::Info, "=== Tau5 is ready! ===");
  });
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
  connect(phxWidget.get(), &PhxWidget::pageLoaded, this, &MainWindow::handleMainWindowLoaded);
  phxWidget->connectToTauPhx(phxUrl);
  setCentralWidget(phxWidget.get());

  if (debugPane) {
    debugPane->setWebView(phxWidget->getWebView());
    QString dashboardUrl = QString("http://localhost:%1/dev/dashboard").arg(port);
    debugPane->setLiveDashboardUrl(dashboardUrl);

    if (beamInstance) {
      QString token = beamInstance->getSessionToken();
      QString consoleUrl = QString("http://localhost:%1/dev/console?token=%2").arg(port).arg(token);
      debugPane->setIexShellUrl(consoleUrl);
    } else {
      Logger::log(Logger::Warning, "BeamInstance is null when setting Tau5 Console URL");
    }

    // Ensure debug pane stays on top if it's visible
    debugPane->raise();
  }

  // Ensure control layer stays on top
  if (controlLayer) {
    controlLayer->raise();
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

  connect(debugPane.get(), &DebugPane::liveDashboardLoaded, this, &MainWindow::handleLiveDashboardLoaded);
  connect(debugPane.get(), &DebugPane::elixirConsoleLoaded, this, &MainWindow::handleElixirConsoleLoaded);
  connect(debugPane.get(), &DebugPane::webDevToolsLoaded, this, &MainWindow::handleWebDevToolsLoaded);
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
    int maxHeight = height();
    int currentHeight = debugPane->height();

    if (currentHeight > maxHeight) {
      currentHeight = maxHeight;
    }

    debugPane->resize(width(), currentHeight);
    debugPane->move(0, height() - currentHeight);
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

void MainWindow::handleMainWindowLoaded()
{
  m_mainWindowLoaded = true;
  Logger::log(Logger::Info, "Main window loaded");
  checkAllComponentsLoaded();
}

void MainWindow::handleLiveDashboardLoaded()
{
  m_liveDashboardLoaded = true;
  Logger::log(Logger::Info, "LiveDashboard loaded");
  checkAllComponentsLoaded();
}

void MainWindow::handleElixirConsoleLoaded()
{
  m_elixirConsoleLoaded = true;
  Logger::log(Logger::Info, "Elixir Console loaded");
  checkAllComponentsLoaded();
}

void MainWindow::handleWebDevToolsLoaded()
{
  m_webDevToolsLoaded = true;
  Logger::log(Logger::Info, "Web Dev Tools loaded");
  checkAllComponentsLoaded();
}

void MainWindow::checkAllComponentsLoaded()
{
  if (m_mainWindowLoaded && m_liveDashboardLoaded &&
      m_elixirConsoleLoaded && m_webDevToolsLoaded) {
    Logger::log(Logger::Info, "All components loaded successfully!");
    emit allComponentsLoaded();
  }
}

