#include <iostream>
#include <QApplication>
#include <QSettings>
#include <QCloseEvent>
#include <QMessageBox>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>
#include <QMoveEvent>
#include <QPropertyAnimation>
#include "mainwindow.h"
#include "widgets/phxwidget.h"
#ifdef BUILD_WITH_DEBUG_PANE
#include "widgets/debugpane.h"
#endif
#include "widgets/controllayer.h"
#include "widgets/loadingoverlay.h"
#include "lib/beam.h"
#include "logger.h"
#include "styles/StyleManager.h"

MainWindow::MainWindow(bool devMode, bool enableDebugPane, QWidget *parent)
    : QMainWindow(parent)
    , beamInstance(nullptr)
    , m_devMode(devMode)
    , m_enableDebugPane(enableDebugPane)
    , m_mainWindowLoaded(false)
#ifdef BUILD_WITH_DEBUG_PANE
    , m_liveDashboardLoaded(!enableDebugPane)  // If debug pane disabled, consider these loaded
    , m_elixirConsoleLoaded(!enableDebugPane)
    , m_webDevToolsLoaded(!enableDebugPane)
#else
    , m_liveDashboardLoaded(true)  // Always true when debug pane not built
    , m_elixirConsoleLoaded(true)
    , m_webDevToolsLoaded(true)
#endif
    , m_allComponentsSignalEmitted(false)
{
  QCoreApplication::setOrganizationName("Tau5");
  QCoreApplication::setApplicationName("Tau5");

  resize(1024, 768);

  QSettings settings;
  if (settings.contains("MainWindow/geometry"))
  {
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
  }

  this->setStyleSheet(StyleManager::mainWindow());

  QMenuBar *menuBar = this->menuBar();
  QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
  QAction *aboutAction = helpMenu->addAction(tr("&About"));
  connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

#ifdef BUILD_WITH_DEBUG_PANE
  if (m_enableDebugPane) {
    initializeDebugPane();
  }
#endif
  initializeControlLayer();

  // Create loading overlay immediately to ensure it's ready for BEAM output
  loadingOverlay = std::make_unique<LoadingOverlay>();
  loadingOverlayStartTime = QDateTime::currentDateTime();
  
  // Show the loading overlay immediately and ensure it appears above the main window
  // Since main window is not shown yet, we need to use screen geometry
  QTimer::singleShot(0, this, [this]() {
    // Use the main window's intended geometry
    QRect targetGeometry = frameGeometry();
    if (!targetGeometry.isValid() || targetGeometry.isEmpty()) {
      // Fallback to screen center if geometry isn't set yet
      QScreen *screen = QApplication::primaryScreen();
      if (screen) {
        QRect screenGeometry = screen->geometry();
        targetGeometry = QRect(
          (screenGeometry.width() - 1024) / 2,
          (screenGeometry.height() - 768) / 2,
          1024, 768
        );
      }
    }
    loadingOverlay->updateGeometry(targetGeometry);
    loadingOverlay->show();
    loadingOverlay->raise();
    loadingOverlay->activateWindow();
  });

  connect(this, &MainWindow::allComponentsLoaded, [this]() {
    Logger::log(Logger::Info, "=== Tau5 is ready! ===");
    
    // Refresh the main browser now that all components are loaded
    if (phxWidget) {
      phxWidget->handleResetBrowser();
    }
    
    if (loadingOverlay) {
      // Calculate remaining time to ensure minimum 8 seconds display
      qint64 elapsedMs = loadingOverlayStartTime.msecsTo(QDateTime::currentDateTime());
      qint64 remainingMs = qMax(0LL, 8000LL - elapsedMs);
      
      QTimer::singleShot(remainingMs + 500, [this]() {
        if (loadingOverlay) {
          // Disconnect BEAM output from loading overlay before fading
          if (beamInstance) {
            disconnect(beamInstance, &Beam::standardOutput,
                      loadingOverlay.get(), &LoadingOverlay::appendLog);
            disconnect(beamInstance, &Beam::standardError,
                      loadingOverlay.get(), &LoadingOverlay::appendLog);
          }
          this->show();
          this->raise();
          this->activateWindow();
          
          // Then start the fade out
          loadingOverlay->fadeOut();
        }
      });
    }
  });
}

MainWindow::~MainWindow() {
  if (loadingOverlay && loadingOverlay->isVisible()) {
    loadingOverlay->close();
  }
}

void MainWindow::setBeamInstance(Beam *beam)
{
  beamInstance = beam;
  if (beamInstance) {
    // Connect to loading overlay for initial display
    if (loadingOverlay) {
      connect(beamInstance, &Beam::standardOutput,
              loadingOverlay.get(), &LoadingOverlay::appendLog);
      connect(beamInstance, &Beam::standardError,
              loadingOverlay.get(), &LoadingOverlay::appendLog);
    }
    
#ifdef BUILD_WITH_DEBUG_PANE
    // Connect to debug pane for later
    if (debugPane) {
      connect(beamInstance, &Beam::standardOutput,
              this, &MainWindow::handleBeamOutput);
      connect(beamInstance, &Beam::standardError,
              this, &MainWindow::handleBeamError);
    }
#endif
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

  phxWidget = std::make_unique<PhxWidget>(m_devMode, this);
  connect(phxWidget.get(), &PhxWidget::pageLoaded, this, &MainWindow::handleMainWindowLoaded);
  phxWidget->connectToTauPhx(phxUrl);
  setCentralWidget(phxWidget.get());

  // Set DevTools availability for the web view
  bool devToolsAvailable = false;
#ifdef BUILD_WITH_DEBUG_PANE
  devToolsAvailable = m_enableDebugPane;
#endif
  phxWidget->getWebView()->setDevToolsAvailable(devToolsAvailable);

#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane) {
    debugPane->setWebView(phxWidget->getWebView());
    QString dashboardUrl = QString("http://localhost:%1/dev/dashboard").arg(port);
    debugPane->setLiveDashboardUrl(dashboardUrl);

    if (beamInstance) {
      QString token = beamInstance->getSessionToken();
      QString consoleUrl = QString("http://localhost:%1/dev/console?token=%2").arg(port).arg(token);
      debugPane->setElixirConsoleUrl(consoleUrl);
    } else {
      Logger::log(Logger::Warning, "BeamInstance is null when setting Tau5 Console URL");
    }

    debugPane->raise();
  }
#endif

  if (controlLayer) {
    controlLayer->raise();
  }
}

#ifdef BUILD_WITH_DEBUG_PANE
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
  connect(debugPane.get(), &DebugPane::restartBeamRequested, this, &MainWindow::handleBeamRestart);
}
#endif

void MainWindow::initializeControlLayer()
{
  controlLayer = std::make_unique<ControlLayer>(this);
  controlLayer->raise();

#ifdef BUILD_WITH_DEBUG_PANE
  controlLayer->setDebugPaneAvailable(m_enableDebugPane);
#else
  controlLayer->setDebugPaneAvailable(false);
#endif

  connect(controlLayer.get(), &ControlLayer::sizeDown, this, &MainWindow::handleSizeDown);
  connect(controlLayer.get(), &ControlLayer::sizeUp, this, &MainWindow::handleSizeUp);
  connect(controlLayer.get(), &ControlLayer::openExternalBrowser, this, &MainWindow::handleOpenExternalBrowser);
  connect(controlLayer.get(), &ControlLayer::resetBrowser, this, &MainWindow::handleResetBrowser);
  connect(controlLayer.get(), &ControlLayer::toggleConsole, this, &MainWindow::toggleConsole);
}

void MainWindow::toggleConsole()
{
#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane) {
    debugPane->toggle();
  }
#endif
}

void MainWindow::handleBeamOutput(const QString &output)
{
#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane) {
    debugPane->appendOutput(output, false);
  }
#else
  Q_UNUSED(output);
#endif
}

void MainWindow::handleBeamError(const QString &error)
{
#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane) {
    debugPane->appendOutput(error, true);
  }
#else
  Q_UNUSED(error);
#endif
}

void MainWindow::handleGuiLog(const QString &message, bool isError)
{
#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane) {
    debugPane->appendGuiLog(message, isError);
  }
#else
  Q_UNUSED(message);
  Q_UNUSED(isError);
#endif
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
  QMainWindow::resizeEvent(event);

#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane && debugPane->isVisible()) {
    int maxHeight = height();
    int currentHeight = debugPane->height();

    if (currentHeight > maxHeight) {
      currentHeight = maxHeight;
    }

    debugPane->resize(width(), currentHeight);
    debugPane->move(0, height() - currentHeight);
  }
#endif

  if (controlLayer) {
    controlLayer->positionControls();
    controlLayer->raise();
  }

  if (loadingOverlay && loadingOverlay->isVisible()) {
    QRect globalGeometry = geometry();
    globalGeometry.moveTopLeft(mapToGlobal(QPoint(0, 0)));
    loadingOverlay->updateGeometry(globalGeometry);
  }
}

void MainWindow::moveEvent(QMoveEvent *event)
{
  QMainWindow::moveEvent(event);
  
  if (loadingOverlay && loadingOverlay->isVisible()) {
    QRect globalGeometry = geometry();
    globalGeometry.moveTopLeft(mapToGlobal(QPoint(0, 0)));
    loadingOverlay->updateGeometry(globalGeometry);
  }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  QSettings settings;
  settings.setValue("MainWindow/geometry", saveGeometry());

#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane) {
    debugPane->saveSettings();
  }
#endif

  if (loadingOverlay) {
    loadingOverlay->close();
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
#ifdef BUILD_WITH_DEBUG_PANE
  bool debugPaneReady = !m_enableDebugPane || (m_liveDashboardLoaded && m_elixirConsoleLoaded && m_webDevToolsLoaded);
  if (m_mainWindowLoaded && debugPaneReady && !m_allComponentsSignalEmitted) {
#else
  if (m_mainWindowLoaded && !m_allComponentsSignalEmitted) {
#endif
    m_allComponentsSignalEmitted = true;
    emit allComponentsLoaded();
  }
}

void MainWindow::handleBeamRestart()
{
#ifdef BUILD_WITH_DEBUG_PANE
  if (!beamInstance)
  {
    Logger::log(Logger::Error, "Cannot restart BEAM: beamInstance is null");
    return;
  }
  
  Logger::log(Logger::Info, "User requested BEAM restart");
  
  // Disable the restart button to prevent multiple restarts
  if (debugPane)
  {
    debugPane->setRestartButtonEnabled(false);
    
    // Add a clear visual separator in the log
    QString separator = "\n" + QString("=").repeated(60) + "\n";
    debugPane->appendOutput(separator + "       BEAM RESTART IN PROGRESS..." + separator, false);
  }
  
  // Reset component loaded flags
  m_mainWindowLoaded = false;
  m_liveDashboardLoaded = false;
  m_elixirConsoleLoaded = false;
  m_webDevToolsLoaded = false;
  m_allComponentsSignalEmitted = false;
  
  // Connect to the restart complete signal
  QObject *context = new QObject();
  connect(beamInstance, &Beam::restartComplete, context, [this, context]() {
    Logger::log(Logger::Info, "BEAM restart complete, reconnecting to server...");
    
    // Re-enable the restart button
    if (debugPane)
    {
      debugPane->setRestartButtonEnabled(true);
      
      // Add completion message
      QString separator = "\n" + QString("=").repeated(60) + "\n";
      debugPane->appendOutput(separator + "       BEAM RESTART COMPLETE!" + separator, false);
    }
    
    // Get the new session token
    QString newToken = beamInstance->getSessionToken();
    
    // Reconnect the Phoenix widget
    if (phxWidget)
    {
      phxWidget->handleResetBrowser();
    }
    
    // Update the Elixir Console URL with the new token
    if (debugPane && beamInstance)
    {
      // Use the port stored in the Beam instance
      quint16 port = beamInstance->getPort();
      QString consoleUrl = QString("http://localhost:%1/dev/console?token=%2").arg(port).arg(newToken);
      debugPane->setElixirConsoleUrl(consoleUrl);
    }
    
    context->deleteLater();
  });
  
  // Trigger the restart
  beamInstance->restart();
#endif
}

