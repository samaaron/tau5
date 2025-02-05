#include <iostream>
#include <QApplication>
#include <QDir>
#include <QTcpServer>
#include <QDebug>
#include "mainwindow.h"
#include "lib/beam.h"

quint16 getFreePort()
{
  QTcpServer server;
  // Bind to port 0 to get an available port
  if (server.listen(QHostAddress::Any, 0))
  {
    quint16 freePort = server.serverPort();
    // Close as we just want to know the port number
    server.close();
    return freePort;
  }
  else
  {
    qDebug() << "Failed to find a free port.";
    return 0; // Return 0 if no free port found
  }
}

int main(int argc, char *argv[])
{
  bool devMode = false;
  std::cout << "Starting Tau5..." << std::endl;

  if (argc > 1 && std::strcmp(argv[1], "dev") == 0)
  {
    std::cout << "Development mode enabled." << std::endl;
    devMode = true;
  }
  else
  {
    std::cout << "Production mode enabled." << std::endl;
  }

  QApplication::setAttribute(Qt::AA_DontShowIconsInMenus, true);

#if defined(Q_OS_LINUX)
  Q_INIT_RESOURCE(Tau5);
  // linux code goes here
#elif defined(Q_OS_WIN)
  Q_INIT_RESOURCE(Tau5);
  // windows code goes here
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#elif defined(Q_OS_DARWIN)
  // macOS code goes here
#endif

  QApplication app(argc, argv);

  app.setApplicationName(QObject::tr("Tau5"));
  app.setStyle("gtk");

  // Get a free port
  quint16 port = getFreePort();
  qDebug() << "Using port: " << port;
  QString appDirPath = QCoreApplication::applicationDirPath();
  QDir dir(appDirPath);

  if (devMode)
  {
    dir.cd("../../../../../server");
    QString basePath = dir.absolutePath();
    Beam *beam = new Beam(&app, basePath, "tau5", "0.1.0", port);
    beam->startElixirServerDev();
  }
  else
  {
    dir.cd("../Resources");
    QString basePath = dir.absolutePath();
    Beam *beam = new Beam(&app, basePath, "tau5", "0.1.0", port);
    beam->startElixirServerProd();
  }

  MainWindow mainWindow(port);
  mainWindow.show();

  return app.exec();
}
