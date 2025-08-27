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
#include "widgets/mainphxwidget.h"
#ifdef BUILD_WITH_DEBUG_PANE
#include "widgets/debugpane.h"
#endif
#include "widgets/controllayer.h"
#include "widgets/consoleoverlay.h"
#include "widgets/transitionoverlay.h"
#include "shared/beam.h"
#include "shared/tau5logger.h"
#include "styles/StyleManager.h"

#ifndef Q_OS_MACOS
#include "widgets/customtitlebar.h"
#include <QWKWidgets/widgetwindowagent.h>
#include <QWKCore/qwkglobal.h>
#endif

MainWindow::MainWindow(bool devMode, bool enableDebugPane, bool enableMcp, bool enableRepl, QWidget *parent)
    : QMainWindow(parent)
    , beamInstance(nullptr)
#ifndef Q_OS_MACOS
    , m_titleBar(nullptr)
    , m_windowAgent(nullptr)
#endif
    , m_devMode(devMode)
    , m_enableDebugPane(enableDebugPane)
    , m_enableMcp(enableMcp)
    , m_enableRepl(enableRepl)
    , m_serverPort(0)
    , m_mainWindowLoaded(false)
    , m_liveDashboardLoaded(!enableDebugPane)
    , m_elixirConsoleLoaded(!enableDebugPane || !enableRepl)
    , m_webDevToolsLoaded(!enableDebugPane)
    , m_allComponentsSignalEmitted(false)
    , m_beamReady(false)
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
  
  transitionOverlay = std::make_unique<TransitionOverlay>(this);
  transitionOverlay->resize(size());
  transitionOverlay->setImmediateOpacity(1.0);
  transitionOverlay->show();
  transitionOverlay->raise();

  QMenuBar *menuBar = this->menuBar();
  if (menuBar) {
    QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    
    #ifndef Q_OS_MACOS
    menuBar->hide();
    #endif
  }

#ifndef Q_OS_MACOS
  m_windowAgent = new QWK::WidgetWindowAgent(this);
  m_windowAgent->setup(this);
  
  QWidget *centralContainer = new QWidget(this);
  QVBoxLayout *centralLayout = new QVBoxLayout(centralContainer);
  centralLayout->setContentsMargins(0, 0, 0, 0);
  centralLayout->setSpacing(0);
  
  m_titleBar = new CustomTitleBar(this);
  centralLayout->addWidget(m_titleBar);
  
  m_windowAgent->setTitleBar(m_titleBar);
  
  m_windowAgent->setSystemButton(QWK::WindowAgentBase::Minimize, m_titleBar->minimizeButton());
  m_windowAgent->setSystemButton(QWK::WindowAgentBase::Maximize, m_titleBar->maximizeButton());
  m_windowAgent->setSystemButton(QWK::WindowAgentBase::Close, m_titleBar->closeButton());
  
  connect(m_titleBar, &CustomTitleBar::minimizeClicked, [this]() {
    this->setWindowState(Qt::WindowMinimized);
  });
  
  connect(m_titleBar, &CustomTitleBar::maximizeClicked, [this]() {
    if (this->windowState() & Qt::WindowMaximized) {
      this->setWindowState(Qt::WindowNoState);
    } else {
      this->setWindowState(Qt::WindowMaximized);
    }
    QTimer::singleShot(50, [this]() {
      m_titleBar->updateMaximizeButton();
    });
  });
  
  connect(m_titleBar, &CustomTitleBar::closeClicked, [this]() {
    this->close();
  });
  
  phxWidget = std::make_unique<MainPhxWidget>(m_devMode, this);
  centralLayout->addWidget(phxWidget.get());
  
  setCentralWidget(centralContainer);
#else
  phxWidget = std::make_unique<MainPhxWidget>(m_devMode, this);
  setCentralWidget(phxWidget.get());
#endif
  
  connect(phxWidget.get(), &PhxWidget::pageLoaded, this, [this]() {
    if (!m_mainWindowLoaded) {
      m_mainWindowLoaded = true;
      Tau5Logger::instance().info( "Shader page loaded and ready");
      checkAllComponentsLoaded();
    }
  });
  
  phxWidget->loadShaderPage();

  consoleOverlay = std::make_unique<ConsoleOverlay>(this);
  consoleOverlay->raise();
  consoleOverlay->show();
  
#ifndef Q_OS_MACOS
  if (m_titleBar && transitionOverlay) {
    transitionOverlay->move(0, m_titleBar->height());
    transitionOverlay->resize(width(), height() - m_titleBar->height());
  }
#else
  if (transitionOverlay) {
    transitionOverlay->resize(size());
  }
#endif
  
  if (transitionOverlay) {
    transitionOverlay->raise();
    transitionOverlay->activateWindow();
  }
  
  QTimer::singleShot(1000, [this]() {
    if (transitionOverlay) {
      transitionOverlay->fadeOut(1000);
    }
  });

  #ifdef BUILD_WITH_DEBUG_PANE
  if (m_enableDebugPane) {
    initializeDebugPane();
  }
  #endif
  
  initializeControlLayer();
  
  if (controlLayer) {
    controlLayer->hide();
  }

  bootStartTime = QDateTime::currentDateTime();

  show();
  raise();
  activateWindow();
  
  if (transitionOverlay) {
    transitionOverlay->raise();
  }

}

MainWindow::~MainWindow() {
}

void MainWindow::setBeamInstance(Beam *beam)
{
  beamInstance = beam;
  if (beamInstance) {
    if (consoleOverlay) {
      connect(beamInstance, &Beam::standardOutput,
              consoleOverlay.get(), &ConsoleOverlay::appendLog);
      connect(beamInstance, &Beam::standardError,
              consoleOverlay.get(), &ConsoleOverlay::appendLog);
    }
    
#ifdef BUILD_WITH_DEBUG_PANE
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
  if (port == 0 || port > 65535) {
    Tau5Logger::instance().error( QString("Invalid port number received: %1").arg(port));
    QMessageBox::critical(this, tr("Server Error"),
                         tr("Invalid server port number: %1").arg(port));
    return false;
  }
  
  m_serverPort = port;
  
  m_beamReady = true;
  Tau5Logger::instance().info( "BEAM server ready, port: " + QString::number(port));
  
  startTransitionToApp();
  
#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane) {
    QString dashboardUrl = QString("http://localhost:%1/dev/dashboard").arg(port);
    debugPane->setLiveDashboardUrl(dashboardUrl);

    if (beamInstance) {
      QString token = beamInstance->getSessionToken();
      QString consoleUrl = QString("http://localhost:%1/dev/console?token=%2").arg(port).arg(token);
      debugPane->setElixirConsoleUrl(consoleUrl);
    }
  }
#endif
  
  return true;
}

bool MainWindow::isElixirReplEnabled()
{
  return m_enableRepl;
}

void MainWindow::startTransitionToApp()
{
  Tau5Logger::instance().info( "BEAM ready, checking timing before transition");
  
  if (m_serverPort == 0) {
    Tau5Logger::instance().error( "Invalid server port for transition");
    return;
  }
  
  qint64 elapsedMs = bootStartTime.msecsTo(QDateTime::currentDateTime());
  qint64 minDisplayMs = 5000;
  qint64 remainingMs = qMax(0LL, minDisplayMs - elapsedMs);
  
  Tau5Logger::instance().info( QString("Boot elapsed: %1ms, waiting %2ms before fade").arg(elapsedMs).arg(remainingMs));
  
  QTimer::singleShot(remainingMs, [this]() {
    Tau5Logger::instance().info( "Starting fade to black transition");
    
    if (transitionOverlay) {
      transitionOverlay->fadeIn(500);
      
      connect(transitionOverlay.get(), &TransitionOverlay::fadeInComplete, 
              this, &MainWindow::onFadeToBlackComplete, Qt::SingleShotConnection);
    }
  });
}

void MainWindow::onFadeToBlackComplete()
{
  Tau5Logger::instance().info( "Fade to black complete, switching to app page");
  
  QUrl phxUrl;
  phxUrl.setScheme("http");
  phxUrl.setHost("localhost");
  phxUrl.setPort(m_serverPort);
  
  if (phxWidget) {
    phxWidget->transitionToApp(phxUrl);
    
    bool devToolsAvailable = false;
#ifdef BUILD_WITH_DEBUG_PANE
    devToolsAvailable = m_enableDebugPane;
#endif
    phxWidget->getWebView()->setDevToolsAvailable(devToolsAvailable);
    
    connect(phxWidget.get(), &PhxWidget::appPageReady, 
            this, &MainWindow::onAppPageReady, Qt::SingleShotConnection);
  }
}

void MainWindow::onAppPageReady()
{
  Tau5Logger::instance().info( "App page ready, fading out overlay");
  
  if (transitionOverlay) {
    transitionOverlay->fadeOut(600);
    
    connect(transitionOverlay.get(), &TransitionOverlay::fadeOutComplete, this, [this]() {
      Tau5Logger::instance().info( "Transition complete, cleaning up overlay");
      if (transitionOverlay) {
        transitionOverlay->hide();
      }
    }, Qt::SingleShotConnection);
  }
  
  if (controlLayer) {
    controlLayer->show();
    controlLayer->raise();
  }
  
  if (consoleOverlay) {
    consoleOverlay->fadeOut();
    connect(consoleOverlay.get(), &ConsoleOverlay::fadeComplete, this, [this]() {
      if (beamInstance) {
        disconnect(beamInstance, &Beam::standardOutput,
                  consoleOverlay.get(), &ConsoleOverlay::appendLog);
        disconnect(beamInstance, &Beam::standardError,
                  consoleOverlay.get(), &ConsoleOverlay::appendLog);
      }
    }, Qt::SingleShotConnection);
  }
  
#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane && phxWidget) {
    debugPane->setWebView(phxWidget->getWebView());
    
    QSettings settings;
    settings.beginGroup("DebugPane");
    bool shouldBeVisible = settings.value("visible", false).toBool();
    settings.endGroup();
    
    if (shouldBeVisible) {
      QTimer::singleShot(DEBUG_PANE_RESTORE_DELAY_MS, [this]() {
        if (debugPane) {
          debugPane->toggle();
        }
      });
    }
  }
#endif
}


#ifdef BUILD_WITH_DEBUG_PANE
void MainWindow::initializeDebugPane()
{
  debugPane = std::make_unique<DebugPane>(this, m_devMode, m_enableMcp, m_enableRepl);

  int defaultHeight = height() / 2;
  debugPane->resize(width(), defaultHeight);
  debugPane->move(0, height() - defaultHeight);
  debugPane->raise();
  debugPane->hide();

  debugPane->restoreSettings();

  QSettings settings;
  settings.beginGroup("DebugPane");
  m_debugPaneShouldBeVisible = settings.value("visible", false).toBool();
  settings.endGroup();

  connect(debugPane.get(), &DebugPane::visibilityChanged,
          this, [this](bool visible) {
            if (controlLayer) {
              controlLayer->setConsoleVisible(visible);
              if (!visible) {
                controlLayer->raise();
              } else {
                debugPane->raise();
              }
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
  
  if (transitionOverlay) {
#ifndef Q_OS_MACOS
    if (m_titleBar) {
      int titleBarHeight = m_titleBar->height();
      transitionOverlay->move(0, titleBarHeight);
      transitionOverlay->resize(width(), height() - titleBarHeight);
    } else {
      transitionOverlay->resize(size());
    }
#else
    transitionOverlay->resize(size());
#endif
  }
}

void MainWindow::moveEvent(QMoveEvent *event)
{
  QMainWindow::moveEvent(event);
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

  QTimer::singleShot(0, []() {
    QApplication::quit();
  });
  
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
  Tau5Logger::instance().info( "Main window loaded");
  checkAllComponentsLoaded();
}

void MainWindow::handleLiveDashboardLoaded()
{
  m_liveDashboardLoaded = true;
  Tau5Logger::instance().info( "LiveDashboard loaded");
  checkAllComponentsLoaded();
}

void MainWindow::handleElixirConsoleLoaded()
{
  m_elixirConsoleLoaded = true;
  Tau5Logger::instance().info( "Elixir Console loaded");
  checkAllComponentsLoaded();
}

void MainWindow::handleWebDevToolsLoaded()
{
  m_webDevToolsLoaded = true;
  Tau5Logger::instance().info( "Web Dev Tools loaded");
  checkAllComponentsLoaded();
}

void MainWindow::checkAllComponentsLoaded()
{
  if (m_mainWindowLoaded && m_beamReady && !m_allComponentsSignalEmitted) {
    m_allComponentsSignalEmitted = true;
    emit allComponentsLoaded();
  } else {
    Tau5Logger::instance().debug( QString("Waiting for components - Window: %1, BEAM: %2")
                .arg(m_mainWindowLoaded)
                .arg(m_beamReady));
  }
}

void MainWindow::handleBeamRestart()
{
#ifdef BUILD_WITH_DEBUG_PANE
  if (!beamInstance)
  {
    Tau5Logger::instance().error( "Cannot restart BEAM: beamInstance is null");
    return;
  }
  
  Tau5Logger::instance().info( "User requested BEAM restart");
  
  if (debugPane)
  {
    debugPane->setRestartButtonEnabled(false);
    
    QString separator = "\n" + QString("=").repeated(60) + "\n";
    debugPane->appendOutput(separator + "       BEAM RESTART IN PROGRESS..." + separator, false);
  }
  
  m_mainWindowLoaded = false;
  m_liveDashboardLoaded = false;
  m_elixirConsoleLoaded = !m_enableRepl;
  m_webDevToolsLoaded = false;
  m_allComponentsSignalEmitted = false;
  
  QObject *context = new QObject();
  connect(beamInstance, &Beam::restartComplete, context, [this, context]() {
    Tau5Logger::instance().info( "BEAM restart complete, reconnecting to server...");
    
    if (debugPane)
    {
      debugPane->setRestartButtonEnabled(true);
      
      QString separator = "\n" + QString("=").repeated(60) + "\n";
      debugPane->appendOutput(separator + "       BEAM RESTART COMPLETE!" + separator, false);
    }
    
    QString newToken = beamInstance->getSessionToken();
    
    if (phxWidget)
    {
      phxWidget->handleResetBrowser();
    }
    
    if (debugPane && beamInstance)
    {
      quint16 port = beamInstance->getPort();
      QString consoleUrl = QString("http://localhost:%1/dev/console?token=%2").arg(port).arg(newToken);
      debugPane->setElixirConsoleUrl(consoleUrl);
    }
    
    context->deleteLater();
  });
  
  beamInstance->restart();
#endif
}

