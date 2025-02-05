#include <iostream>
#include "beam.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QSysInfo>
#include <QOperatingSystemVersion>

Beam::Beam(QObject *parent, const QString &basePath, const QString &appName, const QString &version, quint16 port)
    : QObject(parent), appBasePath(basePath), process(new QProcess(this)) // Properly initialize process here
{
  appPort = port;
  releaseRoot = QFileInfo(QString("%1/_build/prod/rel/%2/").arg(basePath).arg(appName)).absoluteFilePath();
  releasePath = QFileInfo(QString("%1/_build/prod/rel/%2/releases/%3").arg(basePath).arg(appName).arg(version)).absoluteFilePath();
  releaseSysPath = QFileInfo(QString("%1/_build/prod/rel/%2/releases/%3/sys").arg(basePath).arg(appName).arg(version)).absoluteFilePath();
  releaseStartPath = QFileInfo(QString("%1/_build/prod/rel/%2/releases/%3/start").arg(basePath).arg(appName).arg(version)).absoluteFilePath();
  releaseVmArgsPath = QFileInfo(QString("%1/_build/prod/rel/%2/releases/%3/vm.args").arg(basePath).arg(appName).arg(version)).absoluteFilePath();
  releaseLibPath = QFileInfo(QString("%1/_build/prod/rel/%2/lib").arg(basePath).arg(appName)).absoluteFilePath();
  releaseErlBinPath = QFileInfo(QString("%1/_build/prod/rel/%2/erts-15.0.1/bin/erl").arg(basePath).arg(appName)).absoluteFilePath();
}

Beam::~Beam()
{
  if (process)
  {
    if (process->state() == QProcess::Running)
    {
      qDebug() << "Attempting to terminate process...";
      process->terminate();                // Try to gracefully terminate
      if (!process->waitForFinished(5000)) // Wait up to 5 seconds
      {
        qDebug() << "Process did not terminate, killing it...";
        process->kill();            // Force kill if terminate didn't work
        process->waitForFinished(); // Ensure the process has exited
      }
    }
  }
}

void Beam::startElixirServerDev()
{

  std::cout << "Starting Elixir server in Development mode" << std::endl;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("TAU5_ENV", "dev");
  env.insert("TAU5_TOKEN", "abcd");
  QString portStr = QString::number(appPort);
  env.insert("PORT", portStr);

  process->setProcessEnvironment(env);

  process->setWorkingDirectory(appBasePath);
  qDebug() << "Working directory: " << process->workingDirectory();
  QString cmd = "mix";
  QStringList args = {"phx.server"};
  startProcess(cmd, args);
}

void Beam::startElixirServerProd()
{
  std::cout << "Starting Elixir server in Production mode" << std::endl;

  QStringList env;
  env << QString("TAU5_ENV=prod")
      << QString("TAU5_TOKEN=abcd")
      << QString("PORT=%1").arg(appPort)
      << QString("RELEASE_SYS_CONFIG=%1").arg(releaseSysPath)
      << QString("RELEASE_ROOT=%1").arg(releaseRoot)
      << QString("SECRET_KEY_BASE=plksdjflsdjflsdjaflaskdjflsdkfjlsdkfjlsdakfjldskafjdlaskfjdaslkfjdslkfjsdlkafjsldakfj");
  process->setEnvironment(env); // Use setEnvironment instead of setProcessEnvironment

  qDebug() << "F";
  qDebug() << "Working directory: " << appBasePath;

  process->setWorkingDirectory(appBasePath);
  qDebug() << "Working directory2: " << appBasePath;
  QString cmd = releaseErlBinPath;
  QStringList args = {
      "-config", releaseSysPath,
      "-boot", releaseStartPath,
      "-boot_var", "RELEASE_LIB", releaseLibPath,
      "-args_file", releaseVmArgsPath,
      "-noshell",
      "-s", "elixir", "start_cli",
      "-mode", "embedded",
      "-extra", "--no-halt"};

  startProcess(cmd, args);
}

void Beam::startProcess(const QString &cmd, const QStringList &args)
{
  process->start(cmd, args);
  QObject::connect(process, &QProcess::errorOccurred, [](QProcess::ProcessError error)
                   {
    switch (error)
    {
    case QProcess::FailedToStart:
      qDebug() << "Process failed to start.";
      break;
    case QProcess::Crashed:
      qDebug() << "Process crashed after starting.";
      break;
    case QProcess::Timedout:
      qDebug() << "Process timed out.";
      break;
    case QProcess::WriteError:
      qDebug() << "An error occurred while writing to the process.";
      break;
    case QProcess::ReadError:
      qDebug() << "An error occurred while reading from the process.";
      break;
    case QProcess::UnknownError:
    default:
      qDebug() << "An unknown error occurred.";
      break;
    } });

  if (!process->waitForStarted())
  {
    qDebug() << "Error starting BEAM: " << process->errorString();
    qDebug() << "Command called: " << cmd;
    qDebug() << "Args used: " << args;
  }

  connect(process, &QProcess::readyReadStandardOutput, [this]()
          { qDebug() << process->readAllStandardOutput(); });

  connect(process, &QProcess::readyReadStandardError, [this]()
          { qDebug() << process->readAllStandardError(); });
}

bool Beam::isMacOS() const
{
  return (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::MacOS);
}

bool Beam::isWindows() const
{
  return (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Windows);
}
