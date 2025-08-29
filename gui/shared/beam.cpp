#include <iostream>
#include "beam.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QSysInfo>
#include <QOperatingSystemVersion>
#include <QCoreApplication>
#include <QTimer>
#include <QRegularExpression>
#include <QThread>
#include <QUuid>
#include <QHostAddress>
#include <QTcpServer>
#include <QRandomGenerator>
#include <QtConcurrent/QtConcurrent>
#include <QStandardPaths>
#include <QCryptographicHash>
#include "tau5logger.h"
#include "error_codes.h"

#ifndef Q_OS_WIN
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#endif

using namespace Tau5Common;

Beam::Beam(QObject *parent, const QString &basePath, const QString &appName, const QString &version, quint16 port, bool devMode, bool enableMcp, bool enableRepl, DeploymentMode deploymentMode)
    : QObject(parent), appBasePath(basePath), process(new QProcess(this)),
      beamPid(0), heartbeatPort(0), serverReady(false), otpTreeReady(false), devMode(devMode),
      appName(appName), appVersion(version), isRestarting(false),
      enableMcp(enableMcp), enableRepl(enableRepl), useStdinConfig(true), deploymentMode(deploymentMode)
{
  // Tau5Logger MUST be initialized before creating Beam instances
  // as we need the session path for TAU5_LOG_DIR environment variable
  if (!Tau5Logger::isInitialized()) {
    qFatal("Beam: Tau5Logger must be initialized before creating Beam instances");
  }
  
  sessionToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
  heartbeatToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
  appPort = port;
  QByteArray randomBytes(64, 0);
  for (int i = 0; i < 64; ++i) {
    randomBytes[i] = QRandomGenerator::global()->generate();
  }
  secretKeyBase = randomBytes.toBase64();
  heartbeatSocket = new QUdpSocket(this);
  
  // Find an available port for the server to bind to
  // Try binding to verify it's free, then immediately release it
  QUdpSocket portTest;
  for (int attempts = 0; attempts < 100; attempts++) {
    quint16 testPort = QRandomGenerator::global()->bounded(49152, 65535);
    if (portTest.bind(QHostAddress::LocalHost, testPort)) {
      heartbeatPort = testPort;
      portTest.close();
      break;
    }
  }
  
  if (heartbeatPort == 0) {
    // Failed to find free port after 100 attempts
    Tau5Logger::instance().error("Failed to find available UDP port for heartbeat");
    emit standardError("Failed to initialize heartbeat system - no available ports");
    QCoreApplication::exit(static_cast<int>(ExitCode::HEARTBEAT_PORT_FAILED));
    return;
  }
  
  // Only log heartbeat setup errors, not normal operation
  // Tau5Logger::instance().debug(QString("UDP heartbeat: Server will listen on port %1").arg(heartbeatPort));

  connect(process, &QProcess::readyReadStandardOutput,
          this, &Beam::handleStandardOutput);
  connect(process, &QProcess::readyReadStandardError,
          this, &Beam::handleStandardError);

  heartbeatTimer = new QTimer(this);
  QString intervalStr = qEnvironmentVariable("TAU5_HB_GUI_INTERVAL_MS");
  int interval = intervalStr.isEmpty() ? 5000 : intervalStr.toInt();
  if (interval < 1000) interval = 5000; // Minimum 1 second to prevent busy loop
  heartbeatTimer->setInterval(interval);
  // Don't log the timer interval in normal operation
  // Tau5Logger::instance().debug(QString("Heartbeat timer interval set to %1ms").arg(interval));
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
      QCoreApplication::exit(static_cast<int>(ExitCode::SERVER_DIR_NOT_FOUND)); // Exit with specific error code
    }
  }
}

Beam::~Beam()
{
  if (heartbeatTimer && heartbeatTimer->isActive())
  {
    heartbeatTimer->stop();
  }
  
  // Clean up UDP socket
  if (heartbeatSocket) {
    heartbeatSocket->deleteLater();
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

  if (Tau5Logger::isInitialized() && !outputStr.trimmed().isEmpty()) {
    Tau5Logger::instance().log(LogLevel::Info, "beam", outputStr.trimmed());
  }

  QRegularExpression pidRegex("\\[TAU5_BEAM_PID:(\\d+)\\]");
  QRegularExpressionMatch pidMatch = pidRegex.match(outputStr);
  if (pidMatch.hasMatch())
  {
    beamPid = pidMatch.captured(1).toLongLong();
    Tau5Logger::instance().debug( QString("Captured BEAM PID: %1").arg(beamPid));
    serverReady = true;
    heartbeatTimer->start();

    // Emit OTP ready when we receive the PID, as this indicates the server is running
    if (!otpTreeReady) {
      otpTreeReady = true;
      emit otpReady();
    }
  }


  emit standardOutput(outputStr);
}

void Beam::handleStandardError()
{
  QByteArray error = process->readAllStandardError();
  QString errorStr = QString::fromUtf8(error);

  if (Tau5Logger::isInitialized() && !errorStr.trimmed().isEmpty()) {
    Tau5Logger::instance().log(LogLevel::Error, "beam", errorStr.trimmed());
  }

  // Check for address in use error during restart
  if (isRestarting && (errorStr.contains("address already in use") ||
                       errorStr.contains("Address already in use") ||
                       errorStr.contains("EADDRINUSE")))
  {
    Tau5Logger::instance().error( "Port is still in use, restart failed");
    isRestarting = false;
    emit restartComplete(); // Re-enable the button
  }

  emit standardError(errorStr);
}

void Beam::startElixirServerDev()
{
  Tau5Logger::instance().info( "Starting Elixir server in Development mode");
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  
  // Convert enum to string for environment variable
  QString modeString;
  switch(deploymentMode) {
    case DeploymentMode::Gui:
      modeString = "gui";
      break;
    case DeploymentMode::Node:
      modeString = "node";
      break;
    case DeploymentMode::Central:
      modeString = "central";
      break;
  }
  env.insert("TAU5_MODE", modeString);
  env.insert("TAU5_ENV", "dev");
  
  if (useStdinConfig) {
    env.insert("TAU5_USE_STDIN_CONFIG", "true");
    env.insert("TAU5_HEARTBEAT_ENABLED", "true");
    env.insert("PORT", QString::number(appPort));
  } else {
    env.insert("TAU5_SESSION_TOKEN", sessionToken);
    env.insert("TAU5_HEARTBEAT_ENABLED", "true");
    env.insert("TAU5_HEARTBEAT_PORT", QString::number(heartbeatPort));
    env.insert("TAU5_HEARTBEAT_TOKEN", heartbeatToken);
    env.insert("PORT", QString::number(appPort));
  }
  
  env.insert("PHX_HOST", "127.0.0.1");
  env.insert("MIX_ENV", "dev");
  env.insert("RELEASE_DISTRIBUTION", "none");

  // Tau5Logger must be initialized before creating Beam instances
  QString sessionPath = Tau5Logger::instance().currentSessionPath();
  env.insert("TAU5_LOG_DIR", sessionPath);
  Tau5Logger::instance().debug(QString("Setting TAU5_LOG_DIR to: %1").arg(sessionPath));

  // Set MCP and REPL flags based on constructor parameters
  if (enableMcp) {
    env.insert("TAU5_ENABLE_DEV_MCP", "1");
    Tau5Logger::instance().debug( "MCP enabled for Elixir server");
  }
  if (enableRepl) {
    env.insert("TAU5_ENABLE_DEV_REPL", "1");
    Tau5Logger::instance().debug( "REPL enabled for Elixir server");
  }

#ifdef Q_OS_WIN
  QDir dir(QCoreApplication::applicationDirPath());
  dir.cd("../../scripts");
  process->setWorkingDirectory(dir.absolutePath());
  QString cmd = QDir(dir.absolutePath()).filePath("win-start-server.bat");
  QStringList args = {};
#else
  QString serverPath = qEnvironmentVariable("TAU5_SERVER_PATH");
  if (serverPath.isEmpty()) {
    Tau5Logger::instance().error("TAU5_SERVER_PATH not set - cannot start dev server");
    Tau5Logger::instance().error("Please set TAU5_SERVER_PATH environment variable to the server directory path");
    return;
  }
  process->setWorkingDirectory(serverPath);
  QString cmd = "mix";
  QStringList args = {"phx.server"};
#endif
  process->setProcessEnvironment(env);
  startProcess(cmd, args);
}

void Beam::startElixirServerProd()
{
  Tau5Logger::instance().info( "Starting Elixir server in Production mode");

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  
  // Convert enum to string for environment variable
  QString modeString;
  switch(deploymentMode) {
    case DeploymentMode::Gui:
      modeString = "gui";
      break;
    case DeploymentMode::Node:
      modeString = "node";
      break;
    case DeploymentMode::Central:
      modeString = "central";
      break;
  }
  env.insert("TAU5_MODE", modeString);
  env.insert("TAU5_ENV", "prod");
  
  if (useStdinConfig) {
    env.insert("TAU5_USE_STDIN_CONFIG", "true");
    env.insert("TAU5_HEARTBEAT_ENABLED", "true");
    env.insert("PORT", QString::number(appPort));
    env.insert("PHX_SERVER", "1");
  } else {
    env.insert("TAU5_SESSION_TOKEN", sessionToken);
    env.insert("TAU5_HEARTBEAT_ENABLED", "true");
    env.insert("TAU5_HEARTBEAT_PORT", QString::number(heartbeatPort));
    env.insert("TAU5_HEARTBEAT_TOKEN", heartbeatToken);
    env.insert("PORT", QString::number(appPort));
    env.insert("PHX_SERVER", "1");
  }
  
  env.insert("PHX_HOST", "127.0.0.1");
  env.insert("MIX_ENV", "prod");
  env.insert("RELEASE_DISTRIBUTION", "none");

  // Tau5Logger must be initialized before creating Beam instances
  QString sessionPath = Tau5Logger::instance().currentSessionPath();
  env.insert("TAU5_LOG_DIR", sessionPath);
  Tau5Logger::instance().debug(QString("Setting TAU5_LOG_DIR to: %1").arg(sessionPath));

  // Set MCP and REPL flags based on constructor parameters
  if (enableMcp) {
    env.insert("TAU5_ENABLE_DEV_MCP", "1");
    Tau5Logger::instance().debug( "MCP enabled for Elixir server");
  }
  if (enableRepl) {
    env.insert("TAU5_ENABLE_DEV_REPL", "1");
    Tau5Logger::instance().debug( "REPL enabled for Elixir server");
  }

  env.insert("RELEASE_SYS_CONFIG", releaseSysPath);
  env.insert("RELEASE_ROOT", releaseRoot);
  env.insert("RELEASE_DISTRIBUTION", "none");
  
  if (!useStdinConfig) {
    env.insert("SECRET_KEY_BASE", secretKeyBase.isEmpty() ? 
      "plksdjflsdjflsdjaflaskdjflsdkfjlsdkfjlsdakfjldskafjdlaskfjdaslkfjdslkfjsdlkafjsldakfj" : 
      secretKeyBase);
  }

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

void Beam::writeSecretsToStdin()
{
  if (!process || !useStdinConfig) {
    return;
  }
  
  Tau5Logger::instance().debug("Writing secure configuration to process stdin");
  
  QString config;
  config += sessionToken + "\n";
  config += heartbeatToken + "\n";
  config += QString::number(heartbeatPort) + "\n";
  config += secretKeyBase + "\n";
  
  process->write(config.toUtf8());
  process->closeWriteChannel();
  
  Tau5Logger::instance().debug(QString("Secure configuration written (%1 bytes) and stdin closed")
                              .arg(config.toUtf8().size()));
}

void Beam::startProcess(const QString &cmd, const QStringList &args)
{
  Tau5Logger::instance().debug( QString("Server process working directory: %1").arg(process->workingDirectory()));
  Tau5Logger::instance().debug( QString("Starting process: %1 %2").arg(cmd).arg(args.join(" ")));

  connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this](int exitCode, QProcess::ExitStatus status)
          {
            QString message = QString("Process finished with exit code: %1 status: %2")
                            .arg(exitCode)
                            .arg(status == QProcess::NormalExit ? "Normal" : "Crashed");
            Tau5Logger::instance().info( message);
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
    Tau5Logger::instance().error( errorMsg);
    emit standardError(errorMsg);
  });

  process->start(cmd, args);

  if (!process->waitForStarted(5000))  // 5 second timeout
  {
    QString errorMsg = QString("Error starting BEAM: %1\nCommand: %2\nArgs: %3")
                      .arg(process->errorString())
                      .arg(cmd)
                      .arg(args.join(" "));
    Tau5Logger::instance().error( errorMsg);
    emit standardError(errorMsg);
  } else if (useStdinConfig) {
    writeSecretsToStdin();
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
  
  if (!heartbeatSocket)
  {
    Tau5Logger::instance().warning("Cannot send heartbeat - UDP socket not created");
    return;
  }

  QString heartbeatMsg = QString("HEARTBEAT:%1\n").arg(heartbeatToken);
  QByteArray datagram = heartbeatMsg.toUtf8();
  
  qint64 sent = heartbeatSocket->writeDatagram(datagram, QHostAddress::LocalHost, heartbeatPort);
  
  if (sent == -1) {
    Tau5Logger::instance().warning(QString("Failed to send UDP heartbeat: %1")
                                  .arg(heartbeatSocket->errorString()));
  } else {
    // Successfully sent - no need to log every heartbeat
    // Tau5Logger::instance().debug(QString("Sent UDP heartbeat to port %1").arg(heartbeatPort));
  }
}

void Beam::killBeamProcess()
{
  if (beamPid <= 0)
  {
    return;
  }

  Tau5Logger::instance().debug(QString("Attempting to kill BEAM process with PID: %1").arg(beamPid));

#ifdef Q_OS_WIN
  Tau5Logger::instance().debug(QString("Windows: Sending graceful termination to PID: %1").arg(beamPid));

  QProcess gracefulKill;
  gracefulKill.start("taskkill", {"/PID", QString::number(beamPid), "/T"});
  gracefulKill.waitForFinished(1000);
  
  if (gracefulKill.exitCode() != 0) {
    Tau5Logger::instance().debug(QString("taskkill graceful failed with exit code %1").arg(gracefulKill.exitCode()));
  }

  for (int i = 2; i > 0; --i)
  {
    QProcess checkProcess;
    checkProcess.start("tasklist", {"/FI", QString("PID eq %1").arg(beamPid)});
    checkProcess.waitForFinished(500);
    QString output = checkProcess.readAllStandardOutput();

    if (!output.contains(QString::number(beamPid)))
    {
      Tau5Logger::instance().debug(QString("Process %1 terminated gracefully").arg(beamPid));
      beamPid = 0;
      return;
    }

    Tau5Logger::instance().debug(QString("Process %1 still running, waiting... %2").arg(beamPid).arg(i));
    QThread::msleep(500);
  }

  Tau5Logger::instance().debug(QString("Windows: Force killing PID: %1").arg(beamPid));

  QProcess forceKill;
  forceKill.start("taskkill", {"/F", "/PID", QString::number(beamPid), "/T"});
  forceKill.waitForFinished(1000);
  
  if (forceKill.exitCode() != 0) {
    Tau5Logger::instance().debug(QString("taskkill force failed with exit code %1").arg(forceKill.exitCode()));
  }

  QProcess finalCheck;
  finalCheck.start("tasklist", {"/FI", QString("PID eq %1").arg(beamPid)});
  finalCheck.waitForFinished(500);
  QString finalOutput = finalCheck.readAllStandardOutput();

  if (finalOutput.contains(QString::number(beamPid)))
  {
    Tau5Logger::instance().error(QString("Process %1 could not be terminated").arg(beamPid));
  }
  else
  {
    Tau5Logger::instance().debug(QString("Process %1 successfully terminated").arg(beamPid));
    beamPid = 0;
  }

#else
  auto still_running = [](pid_t pid) {
    if (::kill(pid, 0) == 0) return true;
    return errno == EPERM;
  };

  auto pid = static_cast<pid_t>(beamPid);
  
  Tau5Logger::instance().debug(QString("Unix: Sending SIGTERM to PID: %1").arg(beamPid));
  if (::kill(pid, SIGTERM) == -1) {
    if (errno == ESRCH) {
      Tau5Logger::instance().debug(QString("Process %1 not found").arg(beamPid));
      beamPid = 0;
      return;
    } else if (errno == EPERM) {
      Tau5Logger::instance().debug(QString("Permission denied sending SIGTERM to %1").arg(beamPid));
    } else {
      Tau5Logger::instance().debug(QString("Failed to send SIGTERM to %1: %2")
        .arg(beamPid).arg(QString::fromLocal8Bit(std::strerror(errno))));
    }
  }

  for (int i = 0; i < 50 && still_running(pid); ++i) {
    QThread::msleep(100);
  }

  if (still_running(pid)) {
    Tau5Logger::instance().debug(QString("Unix: Sending SIGKILL to PID: %1").arg(beamPid));
    if (::kill(pid, SIGKILL) == -1 && errno != ESRCH) {
      Tau5Logger::instance().debug(QString("Failed to send SIGKILL to %1: %2")
        .arg(beamPid).arg(QString::fromLocal8Bit(std::strerror(errno))));
    }
    
    for (int i = 0; i < 20 && still_running(pid); ++i) {
      QThread::msleep(50);
    }
  }

  if (!still_running(pid)) {
    Tau5Logger::instance().debug(QString("Process %1 terminated").arg(beamPid));
    beamPid = 0;
  } else {
    (void)::kill(pid, 0);
    Tau5Logger::instance().warning(QString("Process %1 still running after SIGKILL (errno=%2: %3)")
      .arg(beamPid).arg(errno).arg(QString::fromLocal8Bit(std::strerror(errno))));
  }
#endif
}

void Beam::restart()
{
  Tau5Logger::instance().info( "Restarting BEAM process...");

  if (isRestarting)
  {
    Tau5Logger::instance().warning( "Restart already in progress");
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
    Tau5Logger::instance().info( "Terminating BEAM process by PID (in background thread)...");

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
    Tau5Logger::instance().warning( "continueRestart called but not restarting");
    return;
  }

  Tau5Logger::instance().info( "Continuing BEAM restart...");

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
    Tau5Logger::instance().info( QString("Port %1 is now available, starting new BEAM process").arg(appPort));
    retryCount = 0;
    startNewBeamProcess();
  }
  else if (retryCount < maxRetries)
  {
    retryCount++;
    Tau5Logger::instance().debug( QString("Port %1 still in use, checking again in 500ms... (attempt %2/%3)")
                .arg(appPort).arg(retryCount).arg(maxRetries));
    // Check again in 500ms
    QTimer::singleShot(500, this, &Beam::checkPortAndStartNewProcess);
  }
  else
  {
    Tau5Logger::instance().error( QString("Port %1 still in use after %2 seconds, giving up")
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
    Tau5Logger::instance().warning( "startNewBeamProcess called but not restarting");
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
      Tau5Logger::instance().error( "Failed to start new BEAM process");
      isRestarting = false;
      emit restartComplete(); // Emit even on failure to re-enable button
    }
  });

  sessionToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
  heartbeatToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
  
  QByteArray randomBytes(64, 0);
  for (int i = 0; i < 64; ++i) {
    randomBytes[i] = QRandomGenerator::global()->generate();
  }
  secretKeyBase = randomBytes.toBase64();
  
  Tau5Logger::instance().debug("Generated new secure tokens");

  Tau5Logger::instance().info( "Starting new BEAM process...");
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
      Tau5Logger::instance().error( "BEAM restart timeout - OTP failed to start");
      isRestarting = false;
      emit restartComplete(); // Emit to re-enable button
    }
  });

  // Connect a one-shot timer to emit restartComplete when OTP is ready
  QObject *context = new QObject();
  connect(this, &Beam::otpReady, context, [this, context]() {
    Tau5Logger::instance().info( "BEAM restart complete");
    isRestarting = false;  // Reset the flag
    emit restartComplete();
    context->deleteLater();
  });
}