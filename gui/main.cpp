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
#include "mainwindow.h"
#include "lib/beam.h"
#include "logger.h"
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
      "--autoplay-policy=no-user-gesture-required";
  constexpr const char *CHROMIUM_FLAGS_DEV =
      "--disable-background-timer-throttling "
      "--disable-renderer-backgrounding "
      "--disable-backgrounding-occluded-windows "
      "--disable-features=AudioServiceOutOfProcess "
      "--autoplay-policy=no-user-gesture-required "
      "--remote-debugging-port=9223";
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
    Logger::log(Logger::Error, "Failed to find a free port.");
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
    Logger::log(Logger::Debug, QString("[Qt] %1").arg(msg));
    break;
  case QtInfoMsg:
    Logger::log(Logger::Info, QString("[Qt] %1").arg(msg));
    break;
  case QtWarningMsg:
    Logger::log(Logger::Warning, QString("[Qt] %1").arg(msg));
    break;
  case QtCriticalMsg:
  case QtFatalMsg:
    Logger::log(Logger::Error, QString("[Qt] %1").arg(msg));
    break;
  }
}

bool initializeApplication(QApplication &app, bool devMode, bool enableMcp, bool enableRepl)
{
  originalMessageHandler = qInstallMessageHandler(tau5MessageHandler);
  
  if (devMode && enableMcp) {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", Config::CHROMIUM_FLAGS_DEV);
    Logger::log(Logger::Info, QString("Chrome DevTools Protocol enabled on port %1").arg(Config::DEVTOOLS_PORT));
    Logger::log(Logger::Info, "MCP servers enabled (--enable-mcp flag set)");
  } else {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", Config::CHROMIUM_FLAGS);
    if (devMode && !enableMcp) {
      Logger::log(Logger::Info, "Running in dev mode without MCP servers (use --enable-mcp to enable)");
    }
  }

  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);

  Q_INIT_RESOURCE(Tau5);
  app.setApplicationName(Config::APP_NAME);
  app.setStyle("gtk");
  
  // Set global tooltip style
  app.setStyleSheet(StyleManager::tooltip());

  int fontId = QFontDatabase::addApplicationFont(":/fonts/CascadiaCodePL.ttf");
  if (fontId != -1) {
    QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
    Logger::log(Logger::Info, QString("Loaded font families: %1").arg(fontFamilies.join(", ")));
    
#ifdef Q_OS_MACOS
    Logger::log(Logger::Debug, "Available monospace fonts on macOS:");
    QStringList families = QFontDatabase::families();
    for (const QString &family : families) {
      if (family.contains("Cascadia", Qt::CaseInsensitive) || 
          family.contains("Consolas", Qt::CaseInsensitive) ||
          family.contains("Monaco", Qt::CaseInsensitive) ||
          family.contains("Courier", Qt::CaseInsensitive)) {
        Logger::log(Logger::Debug, QString("  - %1").arg(family));
      }
    }
#endif
  } else {
    Logger::log(Logger::Warning, "Failed to load Cascadia Code font from resources");
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

  quint16 port = Config::DEFAULT_PORT;
  Logger::log(Logger::Info, "Starting Tau5...");

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
    Logger::log(Logger::Info, "Development mode enabled.");
  }
  else
  {
    Logger::log(Logger::Info, "Production mode enabled.");
    port = getFreePort();

    if (port == 0)
    {
      QMessageBox::critical(nullptr, "Error", "Failed to allocate port");
      return 1;
    }
  }

  QApplication app(argc, argv);

  if (!initializeApplication(app, devMode, enableMcp, enableRepl))
  {
    QMessageBox::critical(nullptr, "Error", "Failed to initialize application");
    return 1;
  }

  Logger::log(Logger::Info, QString("Using port: %1").arg(port));

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
  Logger::log(Logger::Info, QString("Base path: %1").arg(basePath));

  if (!QDir(basePath).exists())
  {
    QMessageBox::critical(nullptr, "Error", "Server directory not found at: " + basePath);
    return 1;
  }

  std::unique_ptr<Beam> beam = std::make_unique<Beam>(&app, basePath, Config::APP_NAME,
                                                      Config::APP_VERSION, port, devMode, enableMcp, enableRepl);

  MainWindow mainWindow(devMode, enableDebugPane, enableMcp, enableRepl);
  
  QObject::connect(&Logger::instance(), &Logger::logMessage,
                   &mainWindow, &MainWindow::handleGuiLog);
  
  mainWindow.setBeamInstance(beam.get());
  
  Logger::log(Logger::Info, getTau5Logo());
  
  Logger::log(Logger::Info, "GUI Logger connected successfully");
#ifdef BUILD_WITH_DEBUG_PANE
  if (!enableDebugPane) {
    Logger::log(Logger::Info, "Debug pane disabled via command line");
  }
#else
  Logger::log(Logger::Info, "Debug pane not included in build");
#endif
  Logger::log(Logger::Info, "Waiting for OTP supervision tree to start...");
  Logger::log(Logger::Debug, "Debug messages are enabled");

  QObject::connect(beam.get(), &Beam::otpReady, [&mainWindow, port]() {
    Logger::log(Logger::Info, "OTP supervision tree ready, connecting to server...");
    if (!mainWindow.connectToServer(port))
    {
      QMessageBox::critical(nullptr, "Error", "Failed to connect to server");
      QApplication::quit();
    }
  });

#if defined(Q_OS_WIN)
  mainWindow.setWindowIcon(QIcon(":/images/app.ico"));
#endif


  return app.exec();
}