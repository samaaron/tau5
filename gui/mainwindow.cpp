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
#include <QUrlQuery>
#include <QTextStream>
#include <QShortcut>
#include <QDir>
#include "mainwindow.h"
#include "widgets/mainphxwidget.h"
#ifdef BUILD_WITH_DEBUG_PANE
#include "widgets/debugpane.h"
#endif
#ifdef BUILD_WITH_TERMINAL_PANE
#include "widgets/terminalpane.h"
#include <QSplitter>
#endif
#include "widgets/controllayer.h"
#include "widgets/consoleoverlay.h"
#include "widgets/transitionoverlay.h"
#include "shared/beam.h"
#include "shared/tau5logger.h"
#include "shared/cli_args.h"
#include "styles/StyleManager.h"

#ifndef Q_OS_MACOS
#include "widgets/customtitlebar.h"
#include <QWKWidgets/widgetwindowagent.h>
#include <QWKCore/qwkglobal.h>
#endif

MainWindow::MainWindow(const Tau5CLI::ServerConfig& config, QWidget *parent)
    : QMainWindow(parent)
    , beamInstance(nullptr)
#ifndef Q_OS_MACOS
    , m_titleBar(nullptr)
    , m_windowAgent(nullptr)
#endif
    , mainContainer(nullptr)
    , m_config(&config)
    , m_devMode(config.getArgs().env == Tau5CLI::CommonArgs::Env::Dev)
    , m_enableDebugPane(config.getArgs().debugPane)
    , m_enableMcp(config.getArgs().mcp)
    , m_enableRepl(config.getArgs().repl)
    , m_allowRemoteAccess(config.getArgs().allowRemoteAccess)
    , m_serverPort(0)
    , m_mainWindowLoaded(false)
    , m_liveDashboardLoaded(!config.getArgs().debugPane)
    , m_elixirConsoleLoaded(!config.getArgs().debugPane || !config.getArgs().repl)
    , m_webDevToolsLoaded(!config.getArgs().debugPane)
    , m_allComponentsSignalEmitted(false)
    , m_beamReady(false)
    , m_channel(config.getArgs().channel)
{
  QCoreApplication::setOrganizationName("Tau5");
  QCoreApplication::setApplicationName("Tau5");

  // Set window title with channel if not 0
  // Use "Tau5Dev" for development builds
#ifdef TAU5_RELEASE_BUILD
  QString appName = "Tau5";
#else
  QString appName = "Tau5Dev";
#endif

  if (m_channel != 0) {
    setWindowTitle(QString("%1 - [%2]").arg(appName).arg(m_channel));
  } else {
    setWindowTitle(appName);
  }

  resize(1024, 768);

  QSettings settings;
  if (settings.contains("MainWindow/geometry"))
  {
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
  }

  this->setStyleSheet(StyleManager::mainWindow());
  
  // Transition overlay should cover the entire window
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

  // Update title to include channel if not 0
  // Use "Tau5Dev" for development builds
#ifdef TAU5_RELEASE_BUILD
  QString titleName = "Tau5";
#else
  QString titleName = "Tau5Dev";
#endif

  if (m_channel != 0) {
    m_titleBar->setTitle(QString("%1 - [%2]").arg(titleName).arg(m_channel));
  } else {
    m_titleBar->setTitle(titleName);
  }

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
  
  // Create a container widget for the web view and its overlays
  webContainer = new QWidget(this);
  QVBoxLayout *webLayout = new QVBoxLayout(webContainer);
  webLayout->setContentsMargins(0, 0, 0, 0);
  webLayout->setSpacing(0);

  phxWidget = std::make_unique<MainPhxWidget>(m_devMode, m_allowRemoteAccess, webContainer);
  webLayout->addWidget(phxWidget.get());

#ifdef BUILD_WITH_TERMINAL_PANE
  // For dev builds, create a horizontal splitter with main container and terminal
  if (m_devMode) {
    // Create main container that will hold web view, debug pane, and nav buttons
    mainContainer = new QWidget(this);
    QVBoxLayout *mainContainerLayout = new QVBoxLayout(mainContainer);
    mainContainerLayout->setContentsMargins(0, 0, 0, 0);
    mainContainerLayout->setSpacing(0);
    mainContainerLayout->addWidget(webContainer);

    // Create horizontal splitter for main container and terminal
    QSplitter *horizontalSplitter = new QSplitter(Qt::Horizontal, this);
    horizontalSplitter->addWidget(mainContainer);
    horizontalSplitter->setChildrenCollapsible(false); // Prevent panels from collapsing

    // Initialize terminal pane but keep it hidden initially
    terminalPane = std::make_unique<TerminalPane>(this);
    terminalPane->setWorkingDirectory(QDir::currentPath());
    horizontalSplitter->addWidget(terminalPane.get());

    // Set stretch factors so main container expands, terminal maintains width
    horizontalSplitter->setStretchFactor(0, 1); // Main container stretches
    horizontalSplitter->setStretchFactor(1, 0); // Terminal maintains fixed width

    // Set initial sizes (80% for main widget, 20% for terminal, but terminal is hidden)
    horizontalSplitter->setSizes(QList<int>() << 800 << 200);
    terminalPane->hide();

    connect(terminalPane.get(), &TerminalPane::closeRequested, [this]() {
      m_terminalPaneVisible = false;
      if (terminalPane) {
        terminalPane->hide();
      }
    });

    centralLayout->addWidget(horizontalSplitter);
  } else {
    centralLayout->addWidget(webContainer);
  }
#else
  centralLayout->addWidget(webContainer);
#endif

  setCentralWidget(centralContainer);
#else
  // Create a container widget for the web view and its overlays
  webContainer = new QWidget(this);
  QVBoxLayout *webLayout = new QVBoxLayout(webContainer);
  webLayout->setContentsMargins(0, 0, 0, 0);
  webLayout->setSpacing(0);

  phxWidget = std::make_unique<MainPhxWidget>(m_devMode, m_allowRemoteAccess, webContainer);
  webLayout->addWidget(phxWidget.get());

#ifdef BUILD_WITH_TERMINAL_PANE
  // For dev builds on macOS, create a horizontal splitter with main container and terminal
  if (m_devMode) {
    // Create main container that will hold web view, debug pane, and nav buttons
    mainContainer = new QWidget(this);
    QVBoxLayout *mainContainerLayout = new QVBoxLayout(mainContainer);
    mainContainerLayout->setContentsMargins(0, 0, 0, 0);
    mainContainerLayout->setSpacing(0);
    mainContainerLayout->addWidget(webContainer);

    // Create horizontal splitter for main container and terminal
    QSplitter *horizontalSplitter = new QSplitter(Qt::Horizontal, this);
    horizontalSplitter->addWidget(mainContainer);
    horizontalSplitter->setChildrenCollapsible(false); // Prevent panels from collapsing

    // Initialize terminal pane but keep it hidden initially
    terminalPane = std::make_unique<TerminalPane>(this);
    terminalPane->setWorkingDirectory(QDir::currentPath());
    horizontalSplitter->addWidget(terminalPane.get());

    // Set stretch factors so main container expands, terminal maintains width
    horizontalSplitter->setStretchFactor(0, 1); // Main container stretches
    horizontalSplitter->setStretchFactor(1, 0); // Terminal maintains fixed width

    // Set initial sizes (80% for main widget, 20% for terminal, but terminal is hidden)
    horizontalSplitter->setSizes(QList<int>() << 800 << 200);
    terminalPane->hide();

    connect(terminalPane.get(), &TerminalPane::closeRequested, [this]() {
      m_terminalPaneVisible = false;
      if (terminalPane) {
        terminalPane->hide();
      }
    });

    setCentralWidget(horizontalSplitter);
  } else {
    setCentralWidget(webContainer);
  }
#else
  setCentralWidget(webContainer);
#endif
#endif
  
  connect(phxWidget.get(), &PhxWidget::pageLoaded, this, [this]() {
    if (!m_mainWindowLoaded) {
      m_mainWindowLoaded = true;
      Tau5Logger::instance().info( "Shader page loaded and ready");
      checkAllComponentsLoaded();
    }
  });

  // Reconnect web view after hard refresh
  connect(phxWidget.get(), &MainPhxWidget::webViewRecreated, this, [this]() {
    Tau5Logger::instance().info("Web view recreated - reinitializing connections");
    initializeWebViewConnections();
  });

  phxWidget->loadShaderPage();

  // Console overlay should be within webContainer to stay within the web view area
  consoleOverlay = std::make_unique<ConsoleOverlay>(webContainer ? webContainer : this);
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

  #ifdef BUILD_WITH_TERMINAL_PANE
  if (m_devMode) {
    initializeTerminalPane();
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

  // Print startup banner to console (always visible regardless of --verbose flag)
  printStartupBanner(port);

  startTransitionToApp();
  
#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane) {
    if (beamInstance) {
      QString token = beamInstance->getSessionToken();
      QString dashboardUrl = QString("http://localhost:%1/dev/dashboard?token=%2").arg(port).arg(token);
      debugPane->setLiveDashboardUrl(dashboardUrl);
      
      QString consoleUrl = QString("http://localhost:%1/dev/console?token=%2").arg(port).arg(token);
      debugPane->setElixirConsoleUrl(consoleUrl);
    } else {
      QString dashboardUrl = QString("http://localhost:%1/dev/dashboard").arg(port);
      debugPane->setLiveDashboardUrl(dashboardUrl);
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
  phxUrl.setPath("/app");
  
  // Add the session token as a query parameter for security
  if (beamInstance) {
    QString token = beamInstance->getSessionToken();
    Tau5Logger::instance().debug(QString("Session token for /app: %1").arg(token.isEmpty() ? "EMPTY" : "PRESENT"));
    if (!token.isEmpty()) {
      QUrlQuery query;
      query.addQueryItem("token", token);
      phxUrl.setQuery(query);
    }
  } else {
    Tau5Logger::instance().warning("No beam instance available for token retrieval");
  }
  
  if (phxWidget) {
    phxWidget->transitionToApp(phxUrl);

    // Dev tools availability is now set in initializeWebViewConnections

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

  // Initialize web view connections (including dev tools)
  initializeWebViewConnections();

#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane) {
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
  // Debug pane should always be attached to webContainer to overlay the web content
  // It should not extend into the terminal area
  debugPane = std::make_unique<DebugPane>(webContainer ? webContainer : this, *m_config);

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
              // Always keep control layer on top
              if (visible) {
                debugPane->raise();
                controlLayer->raise();  // Control layer always on top of debug pane
              } else {
                controlLayer->raise();
              }
            }
          });

  connect(debugPane.get(), &DebugPane::liveDashboardLoaded, this, &MainWindow::handleLiveDashboardLoaded);
  connect(debugPane.get(), &DebugPane::elixirConsoleLoaded, this, &MainWindow::handleElixirConsoleLoaded);
  connect(debugPane.get(), &DebugPane::webDevToolsLoaded, this, &MainWindow::handleWebDevToolsLoaded);
  connect(debugPane.get(), &DebugPane::restartBeamRequested, this, &MainWindow::handleBeamRestart);
}
#endif

void MainWindow::initializeWebViewConnections()
{
  if (!phxWidget) {
    return;
  }

  // Set up dev tools connection
#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane) {
    debugPane->setWebView(phxWidget->getWebView());
  }
#endif

  // Set dev tools availability based on debug pane presence
  bool devToolsAvailable = false;
#ifdef BUILD_WITH_DEBUG_PANE
  devToolsAvailable = m_enableDebugPane;
#endif
  phxWidget->getWebView()->setDevToolsAvailable(devToolsAvailable);
}

#ifdef BUILD_WITH_TERMINAL_PANE
void MainWindow::initializeTerminalPane()
{
  // Add keyboard shortcut to toggle terminal pane (Ctrl+`)
  QShortcut *toggleTerminal = new QShortcut(QKeySequence("Ctrl+`"), this);
  connect(toggleTerminal, &QShortcut::activated, [this]() {
    if (terminalPane) {
      m_terminalPaneVisible = !m_terminalPaneVisible;
      terminalPane->setVisible(m_terminalPaneVisible);
    }
  });

  // Alternative shortcut (Ctrl+Shift+T)
  QShortcut *toggleTerminalAlt = new QShortcut(QKeySequence("Ctrl+Shift+T"), this);
  connect(toggleTerminalAlt, &QShortcut::activated, [this]() {
    if (terminalPane) {
      m_terminalPaneVisible = !m_terminalPaneVisible;
      terminalPane->setVisible(m_terminalPaneVisible);
    }
  });
}
#endif

void MainWindow::initializeControlLayer()
{
  // Control layer should be attached to webContainer to stay within the web view area
  controlLayer = std::make_unique<ControlLayer>(webContainer ? webContainer : this);
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
    // Always keep control layer on top after toggling debug pane
    if (controlLayer) {
      controlLayer->raise();
    }
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

void MainWindow::handleBootLog(const QString &message, bool isError)
{
#ifdef BUILD_WITH_DEBUG_PANE
  if (debugPane) {
    debugPane->appendBootLog(message, isError);
  }
#else
  Q_UNUSED(message);
  Q_UNUSED(isError);
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
  if (debugPane && debugPane->isVisible() && webContainer) {
    int maxHeight = webContainer->height();
    int currentHeight = debugPane->height();

    if (currentHeight > maxHeight) {
      currentHeight = maxHeight;
    }

    debugPane->resize(webContainer->width(), currentHeight);
    debugPane->move(0, webContainer->height() - currentHeight);
  }
#endif

  if (controlLayer) {
    controlLayer->positionControls();
    controlLayer->raise();
  }

#ifdef BUILD_WITH_DEBUG_PANE
  // Ensure control layer stays on top of debug pane when resizing
  if (debugPane && debugPane->isVisible() && controlLayer) {
    controlLayer->raise();
  }
#endif
  
  // Ensure console overlay is properly positioned
  if (consoleOverlay && consoleOverlay->isVisible()) {
    consoleOverlay->positionOverlay();
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
                     tr("Tau5 Tech\n\nby Sam Aaron"));
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

void MainWindow::printStartupBanner(quint16 port)
{
  // Direct output to stdout/stderr - always visible regardless of logger settings
  QTextStream out(stdout);

  // Build the startup banner
  QString banner;
  banner += "\n========================================================\n";

  if (m_devMode) {
    banner += "Tau5 Development Started\n";
  } else {
    banner += "Tau5 Started\n";
  }

  banner += "--------------------------------------------------------\n";

  // Mode
  banner += QString("  Mode:      %1\n").arg(m_devMode ? "development" : "production");

  // Local URL with token
  QString token;
  if (beamInstance) {
    token = beamInstance->getSessionToken();
    banner += QString("  Local:     http://localhost:%1/?token=%2\n").arg(port).arg(token);
  } else {
    banner += QString("  Local:     http://localhost:%1/\n").arg(port);
  }

  // Process PIDs
  banner += QString("  GUI PID:   %1\n").arg(QCoreApplication::applicationPid());
  if (beamInstance) {
    banner += QString("  BEAM PID:  %1\n").arg(beamInstance->getBeamPid());
  }

  // Log location
  banner += QString("  Logs:      %1\n").arg(Tau5Logger::instance().currentSessionPath());

  // Channel (if not default)
  if (m_channel != 0) {
    banner += QString("  Channel:   %1\n").arg(m_channel);
  }

  // MCP info
  if (m_config) {
    const Tau5CLI::CommonArgs& args = m_config->getArgs();
    if (args.mcp) {
      quint16 mcpPort = m_config->getMcpPort();
      QString mcpInfo = QString("Port %1").arg(mcpPort);
      if (args.tidewave) {
        mcpInfo += " (with Tidewave)";
      }
      banner += QString("  MCP:       %1\n").arg(mcpInfo);
    }

    // Chrome DevTools info
    if (args.chromeDevtools) {
      quint16 chromePort = m_config->getChromePort();
      banner += QString("  Chrome CDP: Port %1\n").arg(chromePort);
    }
  }

  // Console URL (if in dev mode with debug pane)
#ifdef BUILD_WITH_DEBUG_PANE
  if (m_devMode && debugPane && beamInstance) {
    QString consoleToken = beamInstance->getSessionToken();
    banner += QString("  Console:   http://localhost:%1/dev/console?token=%2\n").arg(port).arg(consoleToken);
  }
#endif

  banner += "========================================================\n";
  banner += "Press Ctrl+C to stop\n";

  // Write to stdout
  out << banner << Qt::flush;
}

