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
#include "mainwindow.h"
#include "lib/beam.h"
#include "tau5logger.h"
#include "styles/StyleManager.h"

namespace Config
{
  constexpr quint16 DEFAULT_PORT = 5555;
  constexpr quint16 DEVTOOLS_PORT = 9223;
  constexpr const char *APP_NAME = "Tau5";
  constexpr const char *APP_VERSION = "0.1.0";
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


QString getTau5Logo()
{
  return R"(                            ╘
                    ─       ╛▒╛
                     ▐╫       ▄█├
              ─╟╛      █▄      ╪▓▀
    ╓┤┤┤┤┤┤┤┤┤  ╩▌      ██      ▀▓▌
     ▐▒   ╬▒     ╟▓╘    ─▓█      ▓▓├
     ▒╫   ▒╪      ▓█     ▓▓─     ▓▓▄
    ╒▒─  │▒       ▓█     ▓▓     ─▓▓─
    ╬▒   ▄▒ ╒    ╪▓═    ╬▓╬     ▌▓▄
    ╥╒   ╦╥     ╕█╒    ╙▓▐     ▄▓╫
               ▐╩     ▒▒      ▀▀
                    ╒╪      ▐▄

        ______           ______
       /_  __/___  __  _/ ____/
        / / / __ `/ / / /___ \
       / / / /_/ / /_/ /___/ /
      /_/  \__,_/\__,_/_____/

     Collaborative Live Coding
           for Everyone

)";
}


quint16 getFreePort()
{
  QTcpServer server;
  if (server.listen(QHostAddress::Any, 0))
  {
    quint16 freePort = server.serverPort();
    server.close();
    return freePort;
  }
  else
  {
    Tau5Logger::instance().error("Failed to find a free port.");
    return 0;
  }
}

bool setupConsoleOutput()
{
#if defined(Q_OS_WIN)
  if (AttachConsole(ATTACH_PARENT_PROCESS))
  {
    FILE *stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONIN$", "r", stdin);

    std::ios::sync_with_stdio();
    return true;
  }
  else if (AttachConsole(-1))
  {
    FILE *stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONIN$", "r", stdin);

    std::ios::sync_with_stdio();
    return true;
  }
  return false;
#else
  return true;
#endif
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

bool initializeApplication(QApplication &app, bool devMode, bool enableMcp, bool enableRepl)
{
  originalMessageHandler = qInstallMessageHandler(tau5MessageHandler);

  if (devMode && enableMcp) {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", Config::CHROMIUM_FLAGS_DEV);
    Tau5Logger::instance().info(QString("Chrome DevTools Protocol enabled on port %1").arg(Config::DEVTOOLS_PORT));
    Tau5Logger::instance().info("MCP servers enabled (--enable-mcp flag set)");
  } else {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", Config::CHROMIUM_FLAGS);
    if (devMode && !enableMcp) {
      Tau5Logger::instance().info("Running in dev mode without MCP servers (use --enable-mcp to enable)");
    }
  }

  QSurfaceFormat format;
  format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  format.setSwapInterval(1);
  format.setRenderableType(QSurfaceFormat::OpenGL);

#ifdef Q_OS_LINUX
  // Linux: Use NoProfile which lets Qt choose the best available
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
  app.setApplicationName(Config::APP_NAME);
  app.setStyle("gtk");

  // Set global tooltip style
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
  bool devMode = false;
  bool enableDebugPane = true;
  bool enableMcp = false;
  bool enableRepl = false;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "dev") == 0) {
      devMode = true;
    } else if (std::strcmp(argv[i], "--no-debug-pane") == 0) {
      enableDebugPane = false;
    } else if (std::strcmp(argv[i], "--enable-mcp") == 0) {
      enableMcp = true;
    } else if (std::strcmp(argv[i], "--enable-repl") == 0) {
      enableRepl = true;
    } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      std::cout << "Usage: tau5 [options]\n"
                << "Options:\n"
                << "  dev              Run in development mode\n"
                << "  check            Check if application can start\n"
                << "  --no-debug-pane  Disable debug pane\n"
                << "  --enable-mcp     Enable MCP development servers\n"
                << "  --enable-repl    Enable Elixir REPL console\n"
                << "  --help, -h       Show this help message\n";
      return 0;
    }
  }


  if (devMode)
  {
    setupConsoleOutput();
  }

  Tau5LoggerConfig logConfig;
  logConfig.appName = "gui";
  logConfig.logFiles = {
      {"gui.log", "gui", false},
      {"beam.log", "beam", false}
  };
  logConfig.emitQtSignals = enableDebugPane;
  logConfig.consoleEnabled = true;
  logConfig.consoleColors = devMode;
  logConfig.reuseRecentSession = false;

  QString dataPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  logConfig.baseLogDir = QDir(dataPath).absoluteFilePath("Tau5/logs");

  Tau5Logger::initialize(logConfig);

  quint16 port = Config::DEFAULT_PORT;

  Tau5Logger::instance().info("Starting Tau5...");

  if (argc > 1 && std::strcmp(argv[1], "check") == 0)
  {
#if defined(Q_OS_WIN)
    ExitProcess(EXIT_SUCCESS);
#else
    exit(EXIT_SUCCESS);
#endif
  }
  else if (argc > 1 && std::strcmp(argv[1], "dev") == 0)
  {
    Tau5Logger::instance().info("Development mode enabled.");
  }
  else
  {
    Tau5Logger::instance().info("Production mode enabled.");
    port = getFreePort();

    if (port == 0)
    {
      QMessageBox::critical(nullptr, "Error", "Failed to allocate port");
      return 1;
    }
  }

  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);

#ifdef Q_OS_WIN
  QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL, true);
#endif

  QApplication app(argc, argv);

  if (!initializeApplication(app, devMode, enableMcp, enableRepl))
  {
    QMessageBox::critical(nullptr, "Error", "Failed to initialize application");
    return 1;
  }

  Tau5Logger::instance().info(QString("Using port: %1").arg(port));

  QString appDirPath = QCoreApplication::applicationDirPath();
  QDir dir(appDirPath);

#if defined(Q_OS_WIN)
  dir.cd("../../../server");
#elif defined(Q_OS_MACOS)
  dir.cd("../../../../../server");
#else
  dir.cd("../../server");
#endif

  QString basePath = dir.absolutePath();
  Tau5Logger::instance().info(QString("Base path: %1").arg(basePath));

  if (!QDir(basePath).exists())
  {
    QMessageBox::critical(nullptr, "Error", "Server directory not found at: " + basePath);
    return 1;
  }

  // Create MainWindow first (needed for connections)
  MainWindow mainWindow(devMode, enableDebugPane, enableMcp, enableRepl);

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

  // Create a shared pointer for Beam to manage its lifetime
  std::shared_ptr<Beam> beam;

  Tau5Logger::instance().info("Delaying BEAM server startup by 1 second...");

  // Use QTimer for non-blocking delay
  QTimer::singleShot(2000, [&app, &beam, &mainWindow, basePath, port, devMode, enableMcp, enableRepl]() {
    Tau5Logger::instance().info("Starting BEAM server...");
    beam = std::make_shared<Beam>(&app, basePath, Config::APP_NAME,
                                  Config::APP_VERSION, port, devMode, enableMcp, enableRepl);

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

  // Ensure BEAM is terminated before the app fully exits
  QObject::connect(&app, &QApplication::aboutToQuit, [&beam]() {
    if (beam) {
      beam.reset(); // Explicitly destroy beam while Qt is still running
    }
  });

  return app.exec();
}