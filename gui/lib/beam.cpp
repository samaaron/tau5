#include <iostream>
#include "beam.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QSysInfo>
#include <QOperatingSystemVersion>
#include <QApplication>
#include <QTimer>
#include <QRegularExpression>
#include <QThread>
#include <QUuid>
#include <QTcpServer>
#include <QtConcurrent/QtConcurrent>
#include <QStandardPaths>
#include "../logger.h"

Beam::Beam(QObject *parent, const QString &basePath, const QString &appName, const QString &version, quint16 port, bool devMode, bool enableMcp, bool enableRepl)
    : QObject(parent), appBasePath(basePath), process(new QProcess(this)), 
      beamPid(0), serverReady(false), otpTreeReady(false), devMode(devMode),
      appName(appName), appVersion(version), isRestarting(false),
      enableMcp(enableMcp), enableRepl(enableRepl)
{
  sessionToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
  Logger::log(Logger::Debug, QString("Generated session token: %1").arg(sessionToken));
  appPort = port;

  connect(process, &QProcess::readyReadStandardOutput,
          this, &Beam::handleStandardOutput);
  connect(process, &QProcess::readyReadStandardError,
          this, &Beam::handleStandardError);

  heartbeatTimer = new QTimer(this);
  heartbeatTimer->setInterval(5000);
  connect(heartbeatTimer, &QTimer::timeout, this, &Beam::sendHeartbeat);

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
  if (heartbeatTimer && heartbeatTimer->isActive())
  {
    heartbeatTimer->stop();
  }

  if (beamPid > 0)
  {
    killBeamProcess();
  }
  
  // Clean up the QProcess to avoid "destroyed while still running" warning
  if (process)
  {
    process->disconnect();
    
    if (process->state() != QProcess::NotRunning)
    {
      process->terminate();
      if (!process->waitForFinished(1000))
      {
        process->kill();
        process->waitForFinished(1000);
      }
    }
  }
}

void Beam::handleStandardOutput()
{
  QByteArray output = process->readAllStandardOutput();
  QString outputStr = QString::fromUtf8(output);
  
  QRegularExpression pidRegex("\\[TAU5_BEAM_PID:(\\d+)\\]");
  QRegularExpressionMatch pidMatch = pidRegex.match(outputStr);
  if (pidMatch.hasMatch())
  {
    beamPid = pidMatch.captured(1).toLongLong();
    Logger::log(Logger::Debug, QString("Captured BEAM PID: %1").arg(beamPid));
    serverReady = true;
    heartbeatTimer->start();
  }
  
  if (outputStr.contains("[TAU5_OTP_READY]"))
  {
    otpTreeReady = true;
    emit otpReady();
  }
  emit standardOutput(outputStr);
}

void Beam::handleStandardError()
{
  QByteArray error = process->readAllStandardError();
  QString errorStr = QString::fromUtf8(error);
  
  // Check for address in use error during restart
  if (isRestarting && (errorStr.contains("address already in use") || 
                       errorStr.contains("Address already in use") ||
                       errorStr.contains("EADDRINUSE")))
  {
    Logger::log(Logger::Error, "Port is still in use, restart failed");
    isRestarting = false;
    emit restartComplete(); // Re-enable the button
  }
  
  emit standardError(errorStr);
}

void Beam::startElixirServerDev()
{
  Logger::log(Logger::Info, "Starting Elixir server in Development mode");
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("TAU5_MODE", "desktop");
  env.insert("TAU5_ENV", "dev");
  env.insert("TAU5_SESSION_TOKEN", sessionToken);
  env.insert("TAU5_HEARTBEAT_ENABLED", "true");
  QString portStr = QString::number(appPort);
  env.insert("PORT", portStr);
  env.insert("PHX_HOST", "127.0.0.1");
  env.insert("MIX_ENV", "dev");
  env.insert("RELEASE_DISTRIBUTION", "none");
  
  // Set log directory path for the Elixir server
  QString logsPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  QDir logsDir(logsPath);
  if (!logsDir.exists("Tau5/logs")) {
    logsDir.mkpath("Tau5/logs");
  }
  QString logsDirPath = logsDir.absoluteFilePath("Tau5/logs");
  env.insert("TAU5_LOG_DIR", logsDirPath);
  Logger::log(Logger::Debug, QString("Setting TAU5_LOG_DIR to: %1").arg(logsDirPath));
  
  // Set MCP and REPL flags based on constructor parameters
  if (enableMcp) {
    env.insert("TAU5_ENABLE_DEV_MCP", "1");
    Logger::log(Logger::Debug, "MCP enabled for Elixir server");
  }
  if (enableRepl) {
    env.insert("TAU5_ENABLE_DEV_REPL", "1");
    Logger::log(Logger::Debug, "REPL enabled for Elixir server");
  }

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
  Logger::log(Logger::Info, "Starting Elixir server in Production mode");

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("TAU5_MODE", "desktop");
  env.insert("TAU5_ENV", "prod");
  env.insert("TAU5_SESSION_TOKEN", sessionToken);
  env.insert("TAU5_HEARTBEAT_ENABLED", "true");
  QString portStr = QString::number(appPort);
  env.insert("PORT", portStr);
  env.insert("PHX_HOST", "127.0.0.1");
  env.insert("MIX_ENV", "prod");
  env.insert("RELEASE_DISTRIBUTION", "none");
  env.insert("PHX_SERVER", "1");
  
  // Set log directory path for the Elixir server
  QString logsPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  QDir logsDir(logsPath);
  if (!logsDir.exists("Tau5/logs")) {
    logsDir.mkpath("Tau5/logs");
  }
  QString logsDirPath = logsDir.absoluteFilePath("Tau5/logs");
  env.insert("TAU5_LOG_DIR", logsDirPath);
  Logger::log(Logger::Debug, QString("Setting TAU5_LOG_DIR to: %1").arg(logsDirPath));
  
  // Set MCP and REPL flags based on constructor parameters
  if (enableMcp) {
    env.insert("TAU5_ENABLE_DEV_MCP", "1");
    Logger::log(Logger::Debug, "MCP enabled for Elixir server");
  }
  if (enableRepl) {
    env.insert("TAU5_ENABLE_DEV_REPL", "1");
    Logger::log(Logger::Debug, "REPL enabled for Elixir server");
  }

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
  Logger::log(Logger::Debug, QString("Server process working directory: %1").arg(process->workingDirectory()));
  Logger::log(Logger::Debug, QString("Starting process: %1 %2").arg(cmd).arg(args.join(" ")));

  connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this](int exitCode, QProcess::ExitStatus status)
          {
            QString message = QString("Process finished with exit code: %1 status: %2")
                            .arg(exitCode)
                            .arg(status == QProcess::NormalExit ? "Normal" : "Crashed");
            Logger::log(Logger::Info, message);
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
    Logger::log(Logger::Error, errorMsg);
    emit standardError(errorMsg);
  });

  process->start(cmd, args);

  if (!process->waitForStarted())
  {
    QString errorMsg = QString("Error starting BEAM: %1\nCommand: %2\nArgs: %3")
                      .arg(process->errorString())
                      .arg(cmd)
                      .arg(args.join(" "));
    Logger::log(Logger::Error, errorMsg);
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

void Beam::sendHeartbeat()
{
  if (!serverReady || !process || process->state() != QProcess::Running)
  {
    return;
  }

  process->write("TAU5_HEARTBEAT\n");
}

void Beam::killBeamProcess()
{
  if (beamPid <= 0)
  {
    return;
  }

  qDebug() << "Attempting to kill BEAM process with PID:" << beamPid;

#ifdef Q_OS_WIN
  qDebug() << "Windows: Sending graceful termination to PID:" << beamPid;
  
  // Use QProcess instance to suppress error output
  QProcess gracefulKill;
  gracefulKill.start("taskkill", {"/PID", QString::number(beamPid)});
  gracefulKill.waitForFinished();
  
  // Give it a brief moment to terminate gracefully (reduced from 5 seconds to 2)
  for (int i = 2; i > 0; --i)
  {
    QProcess checkProcess;
    checkProcess.start("tasklist", {"/FI", QString("PID eq %1").arg(beamPid)});
    checkProcess.waitForFinished(500); // Shorter timeout
    QString output = checkProcess.readAllStandardOutput();
    
    if (!output.contains(QString::number(beamPid)))
    {
      qDebug() << "Process" << beamPid << "terminated gracefully";
      return;
    }
    
    qDebug() << "Process" << beamPid << "still running, waiting..." << i;
    QThread::msleep(500); // Shorter wait
  }
  
  qDebug() << "Windows: Force killing PID:" << beamPid;
  
  // Use QProcess instance to suppress error output
  QProcess forceKill;
  forceKill.start("taskkill", {"/F", "/PID", QString::number(beamPid)});
  forceKill.waitForFinished(1000); // Add timeout
  
  // Verify termination
  QProcess finalCheck;
  finalCheck.start("tasklist", {"/FI", QString("PID eq %1").arg(beamPid)});
  finalCheck.waitForFinished(500);
  QString finalOutput = finalCheck.readAllStandardOutput();
  
  if (finalOutput.contains(QString::number(beamPid)))
  {
    qDebug() << "Process" << beamPid << "could not be terminated";
  }
  else
  {
    qDebug() << "Process" << beamPid << "successfully terminated";
  }
  
#else
  qDebug() << "Unix: Sending SIGTERM to PID:" << beamPid;
  int result = QProcess::execute("kill", {"-TERM", QString::number(beamPid)});
  
  if (result != 0)
  {
    qDebug() << "Process" << beamPid << "not found or already terminated";
    return;
  }
  
  for (int i = 5; i > 0; --i)
  {
    result = QProcess::execute("kill", {"-0", QString::number(beamPid)});
    
    if (result != 0)
    {
      qDebug() << "Process" << beamPid << "terminated gracefully";
      return;
    }
    
    qDebug() << "Process" << beamPid << "still running, waiting..." << i;
    QThread::msleep(1000);
  }
  
  qDebug() << "Unix: Sending SIGKILL to PID:" << beamPid;
  QProcess::execute("kill", {"-9", QString::number(beamPid)});
#endif

  // Final check is already performed above in the platform-specific sections
}

void Beam::restart()
{
  Logger::log(Logger::Info, "Restarting BEAM process...");
  
  if (isRestarting)
  {
    Logger::log(Logger::Warning, "Restart already in progress");
    return;
  }
  isRestarting = true;
  
  if (heartbeatTimer && heartbeatTimer->isActive())
  {
    heartbeatTimer->stop();
  }
  
  serverReady = false;
  otpTreeReady = false;
  
  if (process)
  {
    disconnect(process, &QProcess::readyReadStandardOutput, this, &Beam::handleStandardOutput);
    disconnect(process, &QProcess::readyReadStandardError, this, &Beam::handleStandardError);
  }
  
  if (beamPid > 0)
  {
    Logger::log(Logger::Info, "Terminating BEAM process by PID (in background thread)...");
    
    // Run killBeamProcess in a separate thread to avoid blocking the UI
    QFuture<void> future = QtConcurrent::run([this]() {
      killBeamProcess();
    });
    
    // Use a QFutureWatcher to know when it's done
    QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
      beamPid = 0;
      continueRestart();
      watcher->deleteLater();
    });
    watcher->setFuture(future);
  }
  else
  {
    continueRestart();
  }
}

void Beam::continueRestart()
{
  if (!isRestarting)
  {
    Logger::log(Logger::Warning, "continueRestart called but not restarting");
    return;
  }
  
  Logger::log(Logger::Info, "Continuing BEAM restart...");
  
  if (process)
  {
    process->deleteLater();
    process = nullptr;
  }
  
  checkPortAndStartNewProcess();
}

void Beam::checkPortAndStartNewProcess()
{
  static int retryCount = 0;
  const int maxRetries = 20; // 10 seconds max (20 * 500ms)
  
  if (!isRestarting)
  {
    retryCount = 0;
    return;
  }
  
  QTcpServer testServer;
  bool portAvailable = testServer.listen(QHostAddress::LocalHost, appPort);
  testServer.close();
  
  if (portAvailable)
  {
    Logger::log(Logger::Info, QString("Port %1 is now available, starting new BEAM process").arg(appPort));
    retryCount = 0;
    startNewBeamProcess();
  }
  else if (retryCount < maxRetries)
  {
    retryCount++;
    Logger::log(Logger::Debug, QString("Port %1 still in use, checking again in 500ms... (attempt %2/%3)")
                .arg(appPort).arg(retryCount).arg(maxRetries));
    // Check again in 500ms
    QTimer::singleShot(500, this, &Beam::checkPortAndStartNewProcess);
  }
  else
  {
    Logger::log(Logger::Error, QString("Port %1 still in use after %2 seconds, giving up")
                .arg(appPort).arg(maxRetries * 0.5));
    retryCount = 0;
    isRestarting = false;
    emit restartComplete(); // Re-enable the button
  }
}

void Beam::startNewBeamProcess()
{
  if (!isRestarting)
  {
    Logger::log(Logger::Warning, "startNewBeamProcess called but not restarting");
    return;
  }
  
  process = new QProcess(this);
  
  connect(process, &QProcess::readyReadStandardOutput,
          this, &Beam::handleStandardOutput);
  connect(process, &QProcess::readyReadStandardError,
          this, &Beam::handleStandardError);
  
  // Add error handling for process startup
  connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart)
    {
      Logger::log(Logger::Error, "Failed to start new BEAM process");
      isRestarting = false;
      emit restartComplete(); // Emit even on failure to re-enable button
    }
  });
  
  sessionToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
  Logger::log(Logger::Debug, QString("Generated new session token: %1").arg(sessionToken));
  
  Logger::log(Logger::Info, "Starting new BEAM process...");
  if (devMode)
  {
    startElixirServerDev();
  }
  else
  {
    startElixirServerProd();
  }
  
  // Set up a timeout to handle cases where OTP never becomes ready
  QTimer::singleShot(30000, this, [this]() { // 30 second timeout
    if (isRestarting)
    {
      Logger::log(Logger::Error, "BEAM restart timeout - OTP failed to start");
      isRestarting = false;
      emit restartComplete(); // Emit to re-enable button
    }
  });
  
  // Connect a one-shot timer to emit restartComplete when OTP is ready
  QObject *context = new QObject();
  connect(this, &Beam::otpReady, context, [this, context]() {
    Logger::log(Logger::Info, "BEAM restart complete");
    isRestarting = false;  // Reset the flag
    emit restartComplete();
    context->deleteLater();
  });
}