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
#include "shared/health_check.h"
#include "styles/StyleManager.h"

using namespace Tau5Common;

namespace GuiConfig
{
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
  // Base flags for dev mode (port will be appended dynamically)
  constexpr const char *CHROMIUM_FLAGS_DEV_BASE =
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
      "--disable-blink-features=LowLatencyCanvas2dImageChromium ";
      // Note: --remote-debugging-port will be added dynamically
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

bool initializeApplication(QApplication &app, const Tau5CLI::CommonArgs &args)
{
  originalMessageHandler = qInstallMessageHandler(tau5MessageHandler);

  // Chrome DevTools are enabled when requested or via environment
  if (args.chromeDevtools || qgetenv("TAU5_DEVTOOLS_ENABLED") == "true") {
    // Get the DevTools port from environment (set by applyEnvironmentVariables)
    QString devToolsPort = qgetenv("TAU5_DEVTOOLS_PORT");
    if (devToolsPort.isEmpty()) {
      devToolsPort = "9223";  // Fallback default
    }
    
    // Build the Chrome flags with the dynamic port
    QString chromiumFlags = QString(GuiConfig::CHROMIUM_FLAGS_DEV_BASE) + 
                           " --remote-debugging-port=" + devToolsPort;
                           
    // Add platform-specific flags
#ifdef Q_OS_WIN
    chromiumFlags += " --disable-frame-rate-limit"
                     " --disable-features=CalculateNativeWinOcclusion,IntensiveWakeUpThrottling"
                     " --disable-blink-features=IntensiveWakeUpThrottling"
                     " --use-angle=d3d11"
                     " --use-cmd-decoder=passthrough"
                     " --disable-features=RendererCodeIntegrity";
#endif
#ifdef Q_OS_LINUX
    chromiumFlags += " --disable-blink-features=IntensiveWakeUpThrottling"
                     " --use-cmd-decoder=passthrough"
                     " --enable-gpu-memory-buffer-video-frames"
                     " --max-active-webgl-contexts=16";
#endif
#ifdef Q_OS_MACOS
    chromiumFlags += " --disable-blink-features=IntensiveWakeUpThrottling";
#endif
    
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", chromiumFlags.toUtf8());
    Tau5Logger::instance().info(QString("Chrome DevTools Protocol enabled on port %1").arg(devToolsPort));
  } else {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", GuiConfig::CHROMIUM_FLAGS);
  }
  
  // Log MCP configuration
  QString mcpPort = qgetenv("TAU5_MCP_PORT");
  if (!mcpPort.isEmpty() && mcpPort != "0") {
    Tau5Logger::instance().info(QString("MCP endpoint enabled on port %1").arg(mcpPort));
    if (qgetenv("TAU5_TIDEWAVE_ENABLED") == "true") {
      Tau5Logger::instance().info("Tidewave development tools enabled on MCP endpoint");
    }
  }

  if (args.verbose) {
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
                  << "\n"
                  << "Quick Setup:\n"
                  << "  --devtools               All-in-one dev setup (dev mode + MCP + Chrome DevTools + Tidewave)\n"
                  << "\n"
                  << "Environment Selection:\n"
                  << "  --env-dev                Development environment (MIX_ENV=dev)\n"
                  << "  --env-prod               Production environment (MIX_ENV=prod) [default]\n"
                  << "  --env-test               Test environment (MIX_ENV=test)\n"
                  << "\n"
                  << "Port Configuration:\n"
                  << "  --port-local <n>         Local web UI port (default: random)\n"
                  << "  --port-public <n>        Public endpoint port (default: disabled)\n"
                  << "  --port-mcp <n>           MCP services port (default: 5555 when enabled)\n"
                  << "  --port-chrome-dev <n>    Chrome DevTools port (default: 9223 when enabled)\n"
                  << "\n"
                  << "Optional Features:\n"
                  << "  --mcp                    Enable MCP endpoint\n"
                  << "  --tidewave               Add Tidewave to MCP endpoint (implies --mcp)\n"
                  << "  --chrome-devtools        Enable Chrome DevTools\n"
                  << "  --repl                   Enable Elixir REPL (dev mode only)\n"
                  << "  --verbose                Enable verbose logging\n"
                  << "  --debug-pane             Enable debug pane [default]\n"
                  << "\n"
                  << "Disable Features:\n"
                  << "  --no-midi                Disable MIDI support\n"
                  << "  --no-link                Disable Ableton Link support\n"
                  << "  --no-discovery           Disable network discovery\n"
                  << "  --no-nifs                Disable all NIFs (MIDI, Link, and Discovery)\n"
                  << "  --no-debug-pane          Disable debug pane\n"
                  << "\n"
                  << "Other:\n"
                  << "  --server-path <path>     Override server directory path\n"
                  << "  --check                  Verify installation and exit\n"
                  << "  --help, -h               Show this help message\n"
                  << "  --version                Show version information\n"
                  << "\n"
                  << "Tau5 - Desktop application for collaborative live-coding\n"
                  << "Creates music and visuals through code. Includes a full GUI interface.\n"
                  << "\n"
                  << "Note: TAU5_MODE is automatically set to 'gui' for the desktop application.\n";
        return 0;
      }
      if (args.showVersion) {
        std::cout << "tau5 version " << Tau5Common::Config::APP_VERSION << "\n";
        return 0;
      }
      continue;
    }
    
    // All arguments are now handled by shared parser
    else {
      std::cerr << "Unknown option: " << argv[i] << "\n";
      std::cerr << "Use --help to see available options\n";
      return 1;
    }
  }

  // Validate arguments for conflicts and dependencies
  if (!Tau5CLI::validateArguments(args)) {
    std::cerr << "Error: " << args.errorMessage << "\n";
    return 1;
  }

  // Check if we're in dev environment from the parsed arguments
  bool isDevMode = (args.env == Tau5CLI::CommonArgs::Env::Dev);
  if (isDevMode)
  {
    setupConsoleOutput();
  }

  // Apply environment variables based on parsed arguments
  // tau5 binary always sets TAU5_MODE=gui
  Tau5CLI::applyEnvironmentVariables(args, "gui");

  Tau5LoggerConfig logConfig;
  logConfig.appName = "gui";
  logConfig.logFiles = {
      {"gui.log", "gui", false},
      {"beam.log", "beam", false}
  };
  logConfig.emitQtSignals = args.debugPane;
  logConfig.consoleEnabled = true;
  logConfig.consoleColors = isDevMode;
  logConfig.reuseRecentSession = false;
  logConfig.baseLogDir = Tau5Logger::getBaseLogDir();

  quint16 port = args.portLocal ? args.portLocal : 0;  // 0 means allocate random port

  if (args.check)
  {
    // Initialize logger with colors enabled for health check output
    logConfig.consoleColors = true;
    Tau5Logger::initialize(logConfig);
    Tau5Logger::instance().info("Starting Tau5...");
    
    // Set up for headless operation if no display
    if (!qEnvironmentVariableIsSet("DISPLAY")) {
      qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    
    QApplication tempApp(argc, argv);
    
    // Get server base path
    QString basePath = getServerBasePath(args.serverPath);
    
    // Run the shared health check
    Tau5HealthCheck::HealthCheckConfig checkConfig;
    checkConfig.serverPath = basePath;
    checkConfig.binaryName = "tau5";
    checkConfig.isGui = true;
    checkConfig.verbose = args.verbose;
    checkConfig.strictMode = false;  // Could add --check-strict flag later
    checkConfig.runTests = args.verbose;  // Run tests in verbose mode
    checkConfig.testPort = 0;  // Auto-allocate
    
    return Tau5HealthCheck::runHealthCheck(checkConfig);
  }
  
  // Initialize logger for normal operation (not health check)
  Tau5Logger::initialize(logConfig);
  Tau5Logger::instance().info("Starting Tau5...");
  
  if (isDevMode)
  {
    Tau5Logger::instance().info("Development mode enabled.");
    // In dev mode, also allocate a random port for security unless custom port specified
    if (!args.portLocal) {
      auto portHolder = allocatePort(port);
      if (!portHolder || port == 0)
      {
        QMessageBox::critical(nullptr, "Error", "Failed to allocate port");
        return 1;
      }
      Tau5Logger::instance().info(QString("Allocated port %1 for internal endpoint").arg(port));
      // portHolder will be destroyed when going out of scope, releasing the port for the Beam process
    }
  }
  else
  {
    Tau5Logger::instance().info("Production mode enabled.");
    if (!args.portLocal) {
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

  if (!initializeApplication(app, args))
  {
    QMessageBox::critical(nullptr, "Error", "Failed to initialize application");
    return 1;
  }

  Tau5Logger::instance().info(QString("Using port: %1").arg(port));

  QString basePath = getServerBasePath(args.serverPath);
  if (basePath.isEmpty())
  {
    Tau5Logger::instance().error("FATAL: No server path configured");
    Tau5Logger::instance().error("Please specify server location using one of:");
    Tau5Logger::instance().error("  --server-path /path/to/server");
    Tau5Logger::instance().error("  TAU5_SERVER_PATH environment variable");
    QMessageBox::critical(nullptr, "Error", "No server path configured.\n\nPlease use --server-path or set TAU5_SERVER_PATH environment variable.");
    return 1;
  }
  
  Tau5Logger::instance().info(QString("Server path: %1").arg(basePath));

  if (!QDir(basePath).exists())
  {
    Tau5Logger::instance().error(QString("FATAL: Server directory not found: %1").arg(basePath));
    QMessageBox::critical(nullptr, "Error", "Server directory not found:\n" + basePath);
    return 1;
  }

  // Map environment variables to mainwindow constructor parameters
  bool enableMcp = (qgetenv("TAU5_MCP_PORT") != "0" && !qgetenv("TAU5_MCP_PORT").isEmpty());
  bool enableRepl = (qgetenv("TAU5_ELIXIR_REPL_ENABLED") == "true");
  MainWindow mainWindow(isDevMode, args.debugPane, enableMcp, enableRepl);

  if (args.debugPane) {
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
  if (!args.debugPane) {
    Tau5Logger::instance().info("Debug pane disabled via command line");
  }
#else
  Tau5Logger::instance().info("Debug pane not included in build");
#endif

  std::shared_ptr<Beam> beam;

  Tau5Logger::instance().info("Delaying BEAM server startup by 1 second...");

  QTimer::singleShot(Tau5Common::Config::BEAM_STARTUP_DELAY_MS, [&app, &beam, &mainWindow, basePath, port, isDevMode]() {
    Tau5Logger::instance().info("Starting BEAM server...");
    bool beamEnableMcp = (qgetenv("TAU5_MCP_PORT") != "0" && !qgetenv("TAU5_MCP_PORT").isEmpty());
    bool beamEnableRepl = (qgetenv("TAU5_ELIXIR_REPL_ENABLED") == "true");
    beam = std::make_shared<Beam>(&app, basePath, Tau5Common::Config::APP_NAME,
                                  Tau5Common::Config::APP_VERSION, port, isDevMode, beamEnableMcp, beamEnableRepl, Beam::DeploymentMode::Gui);

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