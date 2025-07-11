#include <iostream>
#include <memory>
#include <cstdlib>
#include <QApplication>
#include <QDir>
#include <QTcpServer>
#include <QDebug>
#include <QMessageBox>
#include "mainwindow.h"
#include "lib/beam.h"
#include "logger.h"

namespace Config
{
  constexpr quint16 DEFAULT_PORT = 5555;
  constexpr const char *APP_NAME = "Tau5";
  constexpr const char *APP_VERSION = "0.1.0";
  constexpr const char *CHROMIUM_FLAGS =
      "--disable-background-timer-throttling "
      "--disable-renderer-backgrounding "
      "--disable-backgrounding-occluded-windows "
      "--disable-features=AudioServiceOutOfProcess "
      "--autoplay-policy=no-user-gesture-required";
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
  if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole())
  {
    FILE *stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    return true;
  }
  return false;
#else
  return true;
#endif
}

bool initializeApplication(QApplication &app)
{
  qputenv("QTWEBENGINE_CHROMIUM_FLAGS", Config::CHROMIUM_FLAGS);

  QCoreApplication::setAttribute(Qt::AA_UseOpenGLES, true);
  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, true);

  Q_INIT_RESOURCE(Tau5);
  app.setApplicationName(Config::APP_NAME);
  app.setStyle("gtk");

  return true;
}

int main(int argc, char *argv[])
{

  quint16 port = Config::DEFAULT_PORT;
  bool devMode = false;
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
    devMode = true;
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

  if (devMode)
  {
    setupConsoleOutput();
  }

  QApplication app(argc, argv);

  if (!initializeApplication(app))
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
                                                      Config::APP_VERSION, port, devMode);

  MainWindow mainWindow;
  mainWindow.setBeamInstance(beam.get());

  if (!mainWindow.connectToServer(port))
  {
    QMessageBox::critical(nullptr, "Error", "Failed to connect to server");
    return 1;
  }

#if defined(Q_OS_WIN)
  mainWindow.setWindowIcon(QIcon(":/images/app.ico"));
#endif

  mainWindow.show();

  return app.exec();
}