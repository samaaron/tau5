#include <iostream>
#include <memory>
#include <cstdlib>
#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif
#include <QApplication>
#include <QDir>
#include <QTcpServer>
#include <QDebug>
#include <QMessageBox>
#include <QFontDatabase>
#include <QSurfaceFormat>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>
#include "mainwindow.h"
#include "shared/beam.h"
#include "shared/tau5logger.h"
#include "shared/common.h"
#include "shared/cli_args.h"
#include "styles/StyleManager.h"

using namespace Tau5Common;

namespace GuiConfig
{
  constexpr quint16 DEVTOOLS_PORT = 9223;
  constexpr const char *CHROMIUM_FLAGS =
      "--disable-background-timer-throttling "
      "--disable-renderer-backgrounding "
      "--disable-backgrounding-occluded-windows "
      "--disable-features=AudioServiceOutOfProcess "
      "--autoplay-policy=no-user-gesture-required "
      "--ignore-gpu-blocklist "
      "--enable-gpu-rasterization "
      "--enable-accelerated-2d-canvas "
      "--enable-zero-copy "
      "--disable-gpu-driver-bug-workarounds "
      "--disable-gpu-watchdog "
      "--enable-unsafe-webgpu "
      "--enable-features=CanvasOopRasterization "
      "--force-color-profile=srgb "
      "--disable-partial-raster "
      "--enable-gpu-memory-buffer-compositor-resources "
      "--enable-oop-rasterization "
      "--canvas-oop-rasterization "
      "--num-raster-threads=4 "
      "--enable-webgl-draft-extensions "
      "--webgl-antialiasing-mode=none "
      "--disable-blink-features=LowLatencyCanvas2dImageChromium"
#ifdef Q_OS_WIN
      " --disable-frame-rate-limit"
      " --disable-features=CalculateNativeWinOcclusion,IntensiveWakeUpThrottling"
      " --disable-blink-features=IntensiveWakeUpThrottling"
      " --use-angle=d3d11"
      " --use-cmd-decoder=passthrough"
      " --disable-features=RendererCodeIntegrity"
#endif
#ifdef Q_OS_LINUX
      " --disable-blink-features=IntensiveWakeUpThrottling"
      " --use-cmd-decoder=passthrough"
      " --enable-gpu-memory-buffer-video-frames"
      " --max-active-webgl-contexts=16"
#endif
#ifdef Q_OS_MACOS
      " --disable-blink-features=IntensiveWakeUpThrottling"
#endif
      ;
  constexpr const char *CHROMIUM_FLAGS_DEV =
      "--disable-background-timer-throttling "
      "--disable-renderer-backgrounding "
      "--disable-backgrounding-occluded-windows "
      "--disable-features=AudioServiceOutOfProcess "
      "--autoplay-policy=no-user-gesture-required "
      "--ignore-gpu-blocklist "
      "--enable-gpu-rasterization "
      "--enable-accelerated-2d-canvas "
      "--enable-zero-copy "
      "--disable-gpu-driver-bug-workarounds "
      "--disable-gpu-watchdog "
      "--enable-unsafe-webgpu "
      "--enable-features=CanvasOopRasterization "
      "--force-color-profile=srgb "
      "--disable-partial-raster "
      "--enable-gpu-memory-buffer-compositor-resources "
      "--enable-oop-rasterization "
      "--canvas-oop-rasterization "
      "--num-raster-threads=4 "
      "--enable-webgl-draft-extensions "
      "--webgl-antialiasing-mode=none "
      "--disable-blink-features=LowLatencyCanvas2dImageChromium "
      "--remote-debugging-port=9223"
#ifdef Q_OS_WIN
      " --disable-frame-rate-limit"
      " --disable-features=CalculateNativeWinOcclusion,IntensiveWakeUpThrottling"
      " --disable-blink-features=IntensiveWakeUpThrottling"
      " --use-angle=d3d11"
      " --use-cmd-decoder=passthrough"
      " --disable-features=RendererCodeIntegrity"
#endif
#ifdef Q_OS_LINUX
      " --disable-blink-features=IntensiveWakeUpThrottling"
      " --use-cmd-decoder=passthrough"
      " --enable-gpu-memory-buffer-video-frames"
      " --max-active-webgl-contexts=16"
#endif
#ifdef Q_OS_MACOS
      " --disable-blink-features=IntensiveWakeUpThrottling"
#endif
      ;
}

static QtMessageHandler originalMessageHandler = nullptr;

void tau5MessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
  if (originalMessageHandler) {
    originalMessageHandler(type, context, msg);
  }

  switch (type) {
  case QtDebugMsg:
    Tau5Logger::instance().debug(QString("[Qt] %1").arg(msg));
    break;
  case QtInfoMsg:
    Tau5Logger::instance().info(QString("[Qt] %1").arg(msg));
    break;
  case QtWarningMsg:
    Tau5Logger::instance().warning(QString("[Qt] %1").arg(msg));
    break;
  case QtCriticalMsg:
  case QtFatalMsg:
    Tau5Logger::instance().error(QString("[Qt] %1").arg(msg));
    break;
  }
}

bool initializeApplication(QApplication &app, bool devMode, bool enableMcp, bool enableRepl, bool verboseLogging)
{
  originalMessageHandler = qInstallMessageHandler(tau5MessageHandler);

  if (devMode && enableMcp) {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", GuiConfig::CHROMIUM_FLAGS_DEV);
    Tau5Logger::instance().info(QString("Chrome DevTools Protocol enabled on port %1").arg(GuiConfig::DEVTOOLS_PORT));
    Tau5Logger::instance().info("MCP servers enabled (--enable-mcp flag set)");
  } else {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", GuiConfig::CHROMIUM_FLAGS);
    if (devMode && !enableMcp) {
      Tau5Logger::instance().info("Running in dev mode without MCP servers (use --enable-mcp to enable)");
    }
  }

  if (verboseLogging) {
    Tau5Logger::instance().info("Verbose logging enabled");
  }

  QSurfaceFormat format;
  format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  format.setSwapInterval(1);
  format.setRenderableType(QSurfaceFormat::OpenGL);

#ifdef Q_OS_LINUX
  format.setProfile(QSurfaceFormat::NoProfile);
#else
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setVersion(4, 3);
#endif

  format.setRedBufferSize(8);
  format.setGreenBufferSize(8);
  format.setBlueBufferSize(8);
  format.setAlphaBufferSize(8);
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);

  QSurfaceFormat::setDefaultFormat(format);

  Q_INIT_RESOURCE(Tau5);
  app.setApplicationName(Tau5Common::Config::APP_NAME);
  app.setStyle("gtk");

  app.setStyleSheet(StyleManager::tooltip());

  int fontId = QFontDatabase::addApplicationFont(":/fonts/CascadiaCodePL.ttf");
  if (fontId != -1) {
    QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
    Tau5Logger::instance().info(QString("Loaded font families: %1").arg(fontFamilies.join(", ")));

#ifdef Q_OS_MACOS
    Tau5Logger::instance().debug("Available monospace fonts on macOS:");
    QStringList families = QFontDatabase::families();
    for (const QString &family : families) {
      if (family.contains("Cascadia", Qt::CaseInsensitive) ||
          family.contains("Consolas", Qt::CaseInsensitive) ||
          family.contains("Monaco", Qt::CaseInsensitive) ||
          family.contains("Courier", Qt::CaseInsensitive)) {
        Tau5Logger::instance().debug(QString("  - %1").arg(family));
      }
    }
#endif
  } else {
    Tau5Logger::instance().warning("Failed to load Cascadia Code font from resources");
  }

  return true;
}

int main(int argc, char *argv[])
{
  // Parse command line arguments
  Tau5CLI::CommonArgs args;
  bool enableDebugPane = true;
  bool checkMode = false;

  for (int i = 1; i < argc; ++i) {
    const char* nextArg = (i + 1 < argc) ? argv[i + 1] : nullptr;
    
    // Try parsing as a shared argument first
    if (Tau5CLI::parseSharedArg(argv[i], nextArg, i, args)) {
      if (args.hasError) {
        std::cerr << "Error: " << args.errorMessage << "\n";
        std::cerr << "Use --help to see available options\n";
        return 1;
      }
      if (args.showHelp) {
        std::cout << "Usage: tau5 [options]\n"
                  << "Options:\n"
                  << "  dev              Run in development mode\n"
                  << "  check            Verify production release and BEAM startup\n"
                  << "  --enable-mcp     Enable MCP servers (tau5-gui-dev in dev, hermes in prod)\n"
                  << "  --enable-repl    Enable Elixir REPL console\n"
                  << "  --port <number>  Specify server port (default: 5555 in dev, random in prod)\n"
                  << "  --verbose        Enable verbose logging (show all BEAM output)\n"
                  << "  --no-debug-pane  Disable debug pane\n"
                  << "\n"
                  << "Local I/O Services (requires compiled NIFs):\n"
                  << "  --disable-midi       Disable MIDI support\n"
                  << "  --disable-link       Disable Ableton Link support\n"
                  << "  --disable-discovery  Disable network discovery\n"
                  << "  --disable-all        Disable all local I/O services\n"
                  << "\n"
                  << "  --help, -h       Show this help message\n"
                  << "\n"
                  << "Tau5 - Desktop application for collaborative live-coding\n"
                  << "Creates music and visuals through code. Includes a full GUI interface.\n"
                  << "\n"
                  << "Note: MIDI, Link, and Discovery services require the corresponding NIFs\n"
                  << "to be compiled during the build. If a NIF is not available, the service\n"
                  << "will show as 'Module Missing' in the summary.\n";
        return 0;
      }
      continue;
    }
    
    // GUI-specific arguments
    if (std::strcmp(argv[i], "check") == 0) {
      checkMode = true;
    } else if (std::strcmp(argv[i], "--no-debug-pane") == 0) {
      enableDebugPane = false;
    } else {
      std::cerr << "Unknown option: " << argv[i] << "\n";
      std::cerr << "Use --help to see available options\n";
      return 1;
    }
  }


  if (args.devMode)
  {
    setupConsoleOutput();
  }

  // Apply service disable settings before starting
  Tau5CLI::applyServiceDisables(args);

  Tau5LoggerConfig logConfig;
  logConfig.appName = "gui";
  logConfig.logFiles = {
      {"gui.log", "gui", false},
      {"beam.log", "beam", false}
  };
  logConfig.emitQtSignals = enableDebugPane;
  logConfig.consoleEnabled = true;
  logConfig.consoleColors = args.devMode;
  logConfig.reuseRecentSession = false;

  logConfig.baseLogDir = Tau5Logger::getBaseLogDir();

  Tau5Logger::initialize(logConfig);

  quint16 port = args.customPort ? args.customPort : Tau5Common::Config::DEFAULT_PORT;

  Tau5Logger::instance().info("Starting Tau5...");

  if (checkMode)
  {
    Tau5Logger::instance().info("===============================================");
    Tau5Logger::instance().info("Running Tau5 production mode check");
    Tau5Logger::instance().info("===============================================");

    if (!qEnvironmentVariableIsSet("DISPLAY")) {
      qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    
    QApplication tempApp(argc, argv);
    QString basePath = getServerBasePath();

    QString releaseDir = QString("%1/_build/prod/rel/%2").arg(basePath).arg(Tau5Common::Config::APP_NAME);
    Tau5Logger::instance().info(QString("Checking for production release at: %1").arg(releaseDir));

    if (!QDir(releaseDir).exists()) {
      Tau5Logger::instance().error(QString("Production release not found at: %1").arg(releaseDir));
      Tau5Logger::instance().error(QString("Expected app name: %1").arg(Tau5Common::Config::APP_NAME));
      Tau5Logger::instance().error("CHECK FAILED: Missing production release");
      return 1;
    }

    Tau5Logger::instance().info("Production release found, starting BEAM server...");

    {
      quint16 allocatedPort = 0;
      auto portHolder = allocatePort(allocatedPort);
      if (!portHolder || allocatedPort == 0) {
        Tau5Logger::instance().error("Failed to allocate port for production check");
        return 1;
      }
      
      Beam beam(&tempApp, basePath, Tau5Common::Config::APP_NAME, Tau5Common::Config::APP_VERSION,
                allocatedPort, false, false, false, Beam::DeploymentMode::Gui);

      Tau5Logger::instance().info("Waiting for BEAM server to start (timeout: 10 seconds)...");
      int waitCount = 0;
      while (beam.getSessionToken().isEmpty() && waitCount < 100) {
        QThread::msleep(100);
        QCoreApplication::processEvents();
        waitCount++;
      }

      if (!beam.getSessionToken().isEmpty()) {
        Tau5Logger::instance().info("BEAM server started successfully");
        Tau5Logger::instance().info("Session token generated");
        Tau5Logger::instance().info("Shutting down BEAM server...");
        Tau5Logger::instance().info("===============================================");
        Tau5Logger::instance().info("CHECK PASSED: Production mode is operational");
        Tau5Logger::instance().info("===============================================");
        return 0;
      } else {
        Tau5Logger::instance().error("Failed to start BEAM server within timeout");
        Tau5Logger::instance().error("CHECK FAILED: Production mode startup failed");
        return 1;
      }
    }
  }
  else if (args.devMode)
  {
    Tau5Logger::instance().info("Development mode enabled.");
    if (!args.customPort) {
      port = Tau5Common::Config::DEFAULT_PORT;
    }
  }
  else
  {
    Tau5Logger::instance().info("Production mode enabled.");
    if (!args.customPort) {
      auto portHolder = allocatePort(port);
      if (!portHolder || port == 0)
      {
        QMessageBox::critical(nullptr, "Error", "Failed to allocate port");
        return 1;
      }
      // portHolder will be destroyed when going out of scope, releasing the port for the Beam process
    }
  }

  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);

#ifdef Q_OS_WIN
  QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL, true);
#endif

  QApplication app(argc, argv);

  if (!initializeApplication(app, args.devMode, args.enableMcp, args.enableRepl, args.verboseLogging))
  {
    QMessageBox::critical(nullptr, "Error", "Failed to initialize application");
    return 1;
  }

  Tau5Logger::instance().info(QString("Using port: %1").arg(port));

  QString basePath = getServerBasePath();
  Tau5Logger::instance().info(QString("Base path: %1").arg(basePath));

  if (!QDir(basePath).exists())
  {
    QMessageBox::critical(nullptr, "Error", "Server directory not found at: " + basePath);
    return 1;
  }

  MainWindow mainWindow(args.devMode, enableDebugPane, args.enableMcp, args.enableRepl);

  if (enableDebugPane) {
    QObject::connect(&Tau5Logger::instance(), &Tau5Logger::logMessage,
                     &mainWindow, [&mainWindow](LogLevel level, const QString& category,
                                               const QString& message, const QJsonObject&) {
      bool isError = (level >= LogLevel::Warning);
      QString levelStr;
      switch(level) {
        case LogLevel::Debug: levelStr = "[DEBUG]"; break;
        case LogLevel::Info: levelStr = "[INFO]"; break;
        case LogLevel::Warning: levelStr = "[WARN]"; break;
        case LogLevel::Error: levelStr = "[ERROR]"; break;
        case LogLevel::Critical: levelStr = "[CRITICAL]"; break;
      }
      QString formattedMessage = QString("%1 %2").arg(levelStr).arg(message);

      if (category == "gui" || category.isEmpty()) {
        mainWindow.handleGuiLog(formattedMessage, isError);
      }
    });
  }

  Tau5Logger::instance().info(getTau5Logo());
  Tau5Logger::instance().info("GUI Logger connected successfully");
#ifdef BUILD_WITH_DEBUG_PANE
  if (!enableDebugPane) {
    Tau5Logger::instance().info("Debug pane disabled via command line");
  }
#else
  Tau5Logger::instance().info("Debug pane not included in build");
#endif

  std::shared_ptr<Beam> beam;

  Tau5Logger::instance().info("Delaying BEAM server startup by 1 second...");

  QTimer::singleShot(Tau5Common::Config::BEAM_STARTUP_DELAY_MS, [&app, &beam, &mainWindow, basePath, port, &args]() {
    Tau5Logger::instance().info("Starting BEAM server...");
    beam = std::make_shared<Beam>(&app, basePath, Tau5Common::Config::APP_NAME,
                                  Tau5Common::Config::APP_VERSION, port, args.devMode, args.enableMcp, args.enableRepl, Beam::DeploymentMode::Gui);

    mainWindow.setBeamInstance(beam.get());

    Tau5Logger::instance().info("Waiting for OTP supervision tree to start...");
    Tau5Logger::instance().debug("Debug messages are enabled");

    QObject::connect(beam.get(), &Beam::otpReady, [&mainWindow, port]() {
      Tau5Logger::instance().info("OTP supervision tree ready, connecting to server...");
      if (!mainWindow.connectToServer(port))
      {
        QMessageBox::critical(nullptr, "Error", "Failed to connect to server");
        QApplication::quit();
      }
    });
  });

#if defined(Q_OS_WIN)
  mainWindow.setWindowIcon(QIcon(":/images/app.ico"));
#endif

  QObject::connect(&app, &QApplication::aboutToQuit, [&beam]() {
    if (beam) {
      beam.reset();
    }
  });

  return app.exec();
}