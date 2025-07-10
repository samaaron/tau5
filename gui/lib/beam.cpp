#include <iostream>
#include "beam.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QSysInfo>
#include <QOperatingSystemVersion>
#include <QApplication>

Beam::Beam(QObject *parent, const QString &basePath, const QString &appName, const QString &version, quint16 port, bool devMode)
    : QObject(parent), appBasePath(basePath), process(new QProcess(this))
{
  appPort = port;

  connect(process, &QProcess::readyReadStandardOutput,
          this, &Beam::handleStandardOutput);
  connect(process, &QProcess::readyReadStandardError,
          this, &Beam::handleStandardError);

  if (devMode)
  {
    startElixirServerDev();
  }
  else
  {
    releaseRoot = QFileInfo(QString("%1/_build/prod/rel/%2/").arg(basePath).arg(appName)).absoluteFilePath();
    releaseSysPath = QFileInfo(QString("%1/_build/prod/rel/%2/releases/%3/sys").arg(basePath).arg(appName).arg(version)).absoluteFilePath();
    releaseStartPath = QFileInfo(QString("%1/_build/prod/rel/%2/releases/%3/start").arg(basePath).arg(appName).arg(version)).absoluteFilePath();
    releaseVmArgsPath = QFileInfo(QString("%1/_build/prod/rel/%2/releases/%3/vm.args").arg(basePath).arg(appName).arg(version)).absoluteFilePath();
    releaseLibPath = QFileInfo(QString("%1/_build/prod/rel/%2/lib").arg(basePath).arg(appName)).absoluteFilePath();
#if defined(Q_OS_WIN)
    releaseRoot = releaseRoot.replace("/", "\\");
    releaseSysPath = releaseSysPath.replace("/", "\\");
    releaseStartPath = releaseStartPath.replace("/", "\\");
    releaseVmArgsPath = releaseVmArgsPath.replace("/", "\\");
    releaseLibPath = releaseLibPath.replace("/", "\\");
#endif

    QDir releaseDir(QString("%1/_build/prod/rel/%2").arg(basePath).arg(appName));
    QStringList ertsDirs = releaseDir.entryList(QStringList() << "erts-*", QDir::Dirs | QDir::NoDotAndDotDot);

    if (!ertsDirs.isEmpty())
    {
      QString ertsFolder = ertsDirs.first(); // Pick the first match (assuming there's only one)
#ifdef Q_OS_WIN
      releaseErlBinPath = QFileInfo(QString("%1/%2/bin/erl.exe").arg(releaseDir.absolutePath()).arg(ertsFolder)).absoluteFilePath();
#else
      releaseErlBinPath = QFileInfo(QString("%1/%2/bin/erl").arg(releaseDir.absolutePath()).arg(ertsFolder)).absoluteFilePath();
#endif
      startElixirServerProd();
    }
    else
    {
      qCritical() << "BEAM.cpp - Exiting. No Elixir _build release folder found:" << releaseDir.absolutePath();
      QCoreApplication::exit(1); // Exit with non-zero status to indicate an error
    }
  }
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

void Beam::handleStandardOutput()
{
  QByteArray output = process->readAllStandardOutput();
  QString outputStr = QString::fromUtf8(output);
  qDebug() << outputStr;
  emit standardOutput(outputStr);
}

void Beam::handleStandardError()
{
  QByteArray error = process->readAllStandardError();
  QString errorStr = QString::fromUtf8(error);
  qDebug() << errorStr;
  emit standardError(errorStr);
}

void Beam::startElixirServerDev()
{
  qDebug() << "Starting Elixir server in Development mode";
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("TAU5_ENV", "dev");
  env.insert("TAU5_TOKEN", "abcd");
  QString portStr = QString::number(appPort);
  env.insert("PORT", portStr);
  env.insert("PHX_HOST", "127.0.0.1");
  env.insert("MIX_ENV", "dev");
  env.insert("RELEASE_DISTRIBUTION", "none");

#ifdef Q_OS_WIN
  QDir dir(QCoreApplication::applicationDirPath());
  dir.cd("../../scripts");
  process->setWorkingDirectory(dir.absolutePath());
  QString cmd = QDir(dir.absolutePath()).filePath("win-start-server.bat");
  QStringList args = {};
#else
  process->setWorkingDirectory(appBasePath);
  QString cmd = "mix";
  QStringList args = {"phx.server"};
#endif
  process->setProcessEnvironment(env);
  startProcess(cmd, args);
}

void Beam::startElixirServerProd()
{
  qDebug() << "Starting Elixir server in Production mode";

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("TAU5_ENV", "prod");
  env.insert("TAU5_TOKEN", "abcd");
  QString portStr = QString::number(appPort);
  env.insert("PORT", portStr);
  env.insert("PHX_HOST", "127.0.0.1");
  env.insert("MIX_ENV", "prod");
  env.insert("RELEASE_DISTRIBUTION", "none");
  env.insert("PHX_SERVER", "1");

  env.insert("RELEASE_SYS_CONFIG", releaseSysPath);
  env.insert("RELEASE_ROOT", releaseRoot);
  env.insert("RELEASE_DISTRIBUTION", "none");
  env.insert("SECRET_KEY_BASE", "plksdjflsdjflsdjaflaskdjflsdkfjlsdkfjlsdakfjldskafjdlaskfjdaslkfjdslkfjsdlkafjsldakfj");

  process->setWorkingDirectory(appBasePath);
  process->setProcessEnvironment(env); // Use setEnvironment instead of setProcessEnvironment

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
  qDebug() << "Server process working directory: " << process->workingDirectory();
  qDebug() << "Starting process: " << cmd << " " << args.join(" ");

  connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this](int exitCode, QProcess::ExitStatus status)
          {
            QString message = QString("Process finished with exit code: %1 status: %2")
                            .arg(exitCode)
                            .arg(status == QProcess::NormalExit ? "Normal" : "Crashed");
            qDebug() << message;
            emit standardOutput(message);
          });

  connect(process, &QProcess::errorOccurred, [this](QProcess::ProcessError error)
          {
    QString errorMsg;
    switch (error)
    {
    case QProcess::FailedToStart:
      errorMsg = "Process failed to start.";
      break;
    case QProcess::Crashed:
      errorMsg = "Process crashed after starting.";
      break;
    case QProcess::Timedout:
      errorMsg = "Process timed out.";
      break;
    case QProcess::WriteError:
      errorMsg = "An error occurred while writing to the process.";
      break;
    case QProcess::ReadError:
      errorMsg = "An error occurred while reading from the process.";
      break;
    case QProcess::UnknownError:
    default:
      errorMsg = "An unknown error occurred.";
      break;
    }
    qDebug() << errorMsg;
    emit standardError(errorMsg);
  });

  process->start(cmd, args);

  if (!process->waitForStarted())
  {
    QString errorMsg = QString("Error starting BEAM: %1\nCommand: %2\nArgs: %3")
                      .arg(process->errorString())
                      .arg(cmd)
                      .arg(args.join(" "));
    qDebug() << errorMsg;
    emit standardError(errorMsg);
  }
}

bool Beam::isMacOS() const
{
  return (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::MacOS);
}

bool Beam::isWindows() const
{
  return (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Windows);
}