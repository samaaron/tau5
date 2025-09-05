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
  if (!Tau5Logger::isInitialized()) {
    qFatal("Beam: Tau5Logger must be initialized before creating Beam instances");
  }

  sessionToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
  heartbeatToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
  appPort = port;
  QByteArray randomBytes(64, 0);
  quint32 buffer[16];
  QRandomGenerator::system()->fillRange(buffer, 16);
  memcpy(randomBytes.data(), buffer, 64);
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
      Tau5Logger::instance().error("Failed to find available UDP port for heartbeat");
    emit standardError("Failed to initialize heartbeat system - no available ports");
    QCoreApplication::exit(static_cast<int>(ExitCode::HEARTBEAT_PORT_FAILED));
    return;
  }


  connect(process, &QProcess::readyReadStandardOutput,
          this, &Beam::handleStandardOutput);
  connect(process, &QProcess::readyReadStandardError,
          this, &Beam::handleStandardError);

  heartbeatTimer = new QTimer(this);
  QString intervalStr = qEnvironmentVariable("TAU5_HB_GUI_INTERVAL_MS");
  int interval = intervalStr.isEmpty() ? 5000 : intervalStr.toInt();
  if (interval < 1000) interval = 5000; // Minimum 1 second to prevent busy loop
  heartbeatTimer->setInterval(interval);
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
      QString ertsFolder = ertsDirs.first();
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
      QCoreApplication::exit(static_cast<int>(ExitCode::SERVER_DIR_NOT_FOUND));
    }
  }
}

Beam::~Beam()
{
  if (heartbeatTimer && heartbeatTimer->isActive())
  {
    heartbeatTimer->stop();
  }

  if (heartbeatSocket) {
    heartbeatSocket->deleteLater();
  }

  if (beamPid > 0)
  {
    killBeamProcess();
  }

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

  // Check for combined server info message first (new format)
  QRegularExpression serverInfoRegex("\\[TAU5_SERVER_INFO:PID=(\\d+),PORT=(\\d+)\\]");
  QRegularExpressionMatch serverInfoMatch = serverInfoRegex.match(outputStr);
  if (serverInfoMatch.hasMatch())
  {
    beamPid = serverInfoMatch.captured(1).toLongLong();
    quint16 actualPort = serverInfoMatch.captured(2).toUInt();

    Tau5Logger::instance().debug( QString("Captured server info - PID: %1, Port: %2").arg(beamPid).arg(actualPort));

    if (actualPort > 0) {
      appPort = actualPort;  // Update with actual allocated port
    }

    serverReady = true;
    heartbeatTimer->start();

    if (!otpTreeReady) {
      otpTreeReady = true;
      emit otpReady();
    }

    if (actualPort > 0 && actualPort != appPort) {
      emit actualPortAllocated(actualPort);
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

  if (isRestarting && (errorStr.contains("address already in use") ||
                       errorStr.contains("Address already in use") ||
                       errorStr.contains("EADDRINUSE")))
  {
    Tau5Logger::instance().error( "Port is still in use, restart failed");
    isRestarting = false;
    emit restartComplete();
  }

  emit standardError(errorStr);
}

QProcessEnvironment Beam::createControlledEnvironment()
{
    QProcessEnvironment env;
    // PATH - dev builds inherit from parent, release builds get nothing
#ifndef TAU5_RELEASE_BUILD
 // Dev builds need access to PATH for dev tools (git, etc.)

  QProcessEnvironment sysEnv = QProcessEnvironment::systemEnvironment();
  QString systemPath = sysEnv.value("PATH");
  if (!systemPath.isEmpty()) {
    env.insert("PATH", systemPath);
  }
#endif

  // Home directory for BEAM config files
  env.insert("HOME", QDir::homePath());

#ifdef Q_OS_UNIX
  // Get username from home directory path instead of environment
  QString homePath = QDir::homePath();
  QString userName = homePath.section('/', -1);
  env.insert("USER", userName);
#endif

  // Locale
  env.insert("LANG", "en_US.UTF-8");
  env.insert("LC_ALL", "en_US.UTF-8");
  env.insert("LC_CTYPE", "en_US.UTF-8");

  env.insert("TERM", "xterm-256color");

  // Temp directories
  env.insert("TMPDIR", QDir::tempPath());
  env.insert("TMP", QDir::tempPath());
  env.insert("TEMP", QDir::tempPath());

  // Tau5-specific variables from CLI arguments only
  if (!qgetenv("MIX_ENV").isEmpty()) {
    env.insert("MIX_ENV", qgetenv("MIX_ENV"));
  }
  if (!qgetenv("TAU5_MODE").isEmpty()) {
    env.insert("TAU5_MODE", qgetenv("TAU5_MODE"));
  }

  // MCP configuration
  if (!qgetenv("TAU5_MCP_PORT").isEmpty() && qgetenv("TAU5_MCP_PORT") != "0") {
    env.insert("TAU5_MCP_PORT", qgetenv("TAU5_MCP_PORT"));
    env.insert("TAU5_MCP_ENABLED", qgetenv("TAU5_MCP_ENABLED"));
  }
  if (qgetenv("TAU5_TIDEWAVE_ENABLED") == "true") {
    env.insert("TAU5_TIDEWAVE_ENABLED", "true");
  }

  if (qgetenv("TAU5_ELIXIR_REPL_ENABLED") == "true") {
    env.insert("TAU5_ELIXIR_REPL_ENABLED", "true");
  }

  // NIF configuration
  if (qgetenv("TAU5_MIDI_ENABLED") == "false") {
    env.insert("TAU5_MIDI_ENABLED", "false");
  }
  if (qgetenv("TAU5_LINK_ENABLED") == "false") {
    env.insert("TAU5_LINK_ENABLED", "false");
  }
  if (qgetenv("TAU5_DISCOVERY_ENABLED") == "false") {
    env.insert("TAU5_DISCOVERY_ENABLED", "false");
  }

  if (qgetenv("TAU5_VERBOSE") == "true") {
    env.insert("TAU5_VERBOSE", "true");
  }


  return env;
}

void Beam::startElixirServerDev()
{
  Tau5Logger::instance().info( "Starting Elixir server in Development mode");

  // Use controlled environment instead of system environment
  QProcessEnvironment env = createControlledEnvironment();


  if (useStdinConfig) {
    env.insert("TAU5_USE_STDIN_CONFIG", "true");
    env.insert("TAU5_HEARTBEAT_ENABLED", "true");
    // PORT is now passed via stdin, not environment
  } else {
    // Fallback mode - less secure but necessary for some deployments
    env.insert("TAU5_SESSION_TOKEN", sessionToken);
    env.insert("TAU5_HEARTBEAT_ENABLED", "true");
    env.insert("TAU5_HEARTBEAT_PORT", QString::number(heartbeatPort));
    env.insert("TAU5_HEARTBEAT_TOKEN", heartbeatToken);
    env.insert("PORT", QString::number(appPort));  // Only in fallback mode
  }

  env.insert("PHX_HOST", "127.0.0.1");
  if (env.value("MIX_ENV").isEmpty()) {
    env.insert("MIX_ENV", "dev");
  }
  env.insert("RELEASE_DISTRIBUTION", "none");

  QString sessionPath = Tau5Logger::instance().currentSessionPath();
  env.insert("TAU5_LOG_DIR", sessionPath);
  Tau5Logger::instance().debug(QString("Setting TAU5_LOG_DIR to: %1").arg(sessionPath));

  if (env.value("TAU5_MCP_PORT").toInt() > 0) {
    Tau5Logger::instance().debug(QString("MCP endpoint enabled on port %1").arg(env.value("TAU5_MCP_PORT")));
  }
  if (env.value("TAU5_TIDEWAVE_ENABLED") == "true") {
    Tau5Logger::instance().debug("Tidewave development tools enabled on MCP endpoint");
  }

  if (env.value("TAU5_ELIXIR_REPL_ENABLED") == "true") {
    Tau5Logger::instance().debug("Elixir REPL enabled for development");
  }

#ifdef Q_OS_WIN
  QDir dir(QCoreApplication::applicationDirPath());
  dir.cd("../../scripts");
  process->setWorkingDirectory(dir.absolutePath());
  QString cmd = QDir(dir.absolutePath()).filePath("win-start-server.bat");
  QStringList args = {};
#else
  if (appBasePath.isEmpty()) {
    Tau5Logger::instance().error("Server path not set - cannot start dev server");
    Tau5Logger::instance().error("Please use --server-path argument or set TAU5_SERVER_PATH environment variable");
    return;
  }
  process->setWorkingDirectory(appBasePath);
  QString cmd = "mix";
  QStringList args = {"phx.server"};
#endif
  process->setProcessEnvironment(env);
  startProcess(cmd, args);
}

void Beam::startElixirServerProd()
{
  Tau5Logger::instance().info( "Starting Elixir server in Production mode");

  // Use controlled environment instead of system environment
  QProcessEnvironment env = createControlledEnvironment();

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
  if (env.value("TAU5_MODE").isEmpty()) {
    env.insert("TAU5_MODE", modeString);
  }

  if (useStdinConfig) {
    env.insert("TAU5_USE_STDIN_CONFIG", "true");
    env.insert("TAU5_HEARTBEAT_ENABLED", "true");
    // PORT is now passed via stdin, not environment
    env.insert("PHX_SERVER", "1");
  } else {
    // Fallback mode - less secure but necessary for some deployments
    env.insert("TAU5_SESSION_TOKEN", sessionToken);
    env.insert("TAU5_HEARTBEAT_ENABLED", "true");
    env.insert("TAU5_HEARTBEAT_PORT", QString::number(heartbeatPort));
    env.insert("TAU5_HEARTBEAT_TOKEN", heartbeatToken);
    env.insert("PORT", QString::number(appPort));  // Only in fallback mode
    env.insert("PHX_SERVER", "1");
  }

  env.insert("PHX_HOST", "127.0.0.1");
  if (env.value("MIX_ENV").isEmpty()) {
    env.insert("MIX_ENV", "prod");
  }
  env.insert("RELEASE_DISTRIBUTION", "none");

  QString envSecretKey = env.value("SECRET_KEY_BASE");
  if (!envSecretKey.isEmpty()) {
    Tau5Logger::instance().info("Using provided SECRET_KEY_BASE from environment");
  } else {
    env.insert("SECRET_KEY_BASE", secretKeyBase);
    Tau5Logger::instance().info("Using auto-generated SECRET_KEY_BASE for this session");
  }

  QString sessionPath = Tau5Logger::instance().currentSessionPath();
  env.insert("TAU5_LOG_DIR", sessionPath);
  Tau5Logger::instance().debug(QString("Setting TAU5_LOG_DIR to: %1").arg(sessionPath));

  if (env.value("TAU5_MCP_PORT").toInt() > 0) {
    Tau5Logger::instance().debug(QString("MCP endpoint enabled on port %1").arg(env.value("TAU5_MCP_PORT")));
  }
  if (env.value("TAU5_TIDEWAVE_ENABLED") == "true") {
    Tau5Logger::instance().debug("Tidewave development tools enabled on MCP endpoint");
  }

  if (env.value("TAU5_ELIXIR_REPL_ENABLED") == "true") {
    Tau5Logger::instance().debug("Elixir REPL enabled for development");
  }

  env.insert("RELEASE_SYS_CONFIG", releaseSysPath);
  env.insert("RELEASE_ROOT", releaseRoot);
  env.insert("RELEASE_DISTRIBUTION", "none");

  process->setWorkingDirectory(appBasePath);
  process->setProcessEnvironment(env);

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
  config += QString::number(appPort) + "\n";  // PORT is now a secret
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

  if (!process->waitForStarted(5000))
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

    QFuture<void> future = QtConcurrent::run([this]() {
      killBeamProcess();
    });

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
  const int maxRetries = 20;

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
    QTimer::singleShot(500, this, &Beam::checkPortAndStartNewProcess);
  }
  else
  {
    Tau5Logger::instance().error( QString("Port %1 still in use after %2 seconds, giving up")
                .arg(appPort).arg(maxRetries * 0.5));
    retryCount = 0;
    isRestarting = false;
    emit restartComplete();
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

  connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart)
    {
      Tau5Logger::instance().error( "Failed to start new BEAM process");
      isRestarting = false;
      emit restartComplete();
    }
  });

  sessionToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
  heartbeatToken = QUuid::createUuid().toString(QUuid::WithoutBraces);

  QByteArray randomBytes(64, 0);
  quint32 buffer[16];
  QRandomGenerator::system()->fillRange(buffer, 16);
  memcpy(randomBytes.data(), buffer, 64);
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

  QTimer::singleShot(30000, this, [this]() {
    if (isRestarting)
    {
      Tau5Logger::instance().error( "BEAM restart timeout - OTP failed to start");
      isRestarting = false;
      emit restartComplete();
    }
  });

  QObject *context = new QObject();
  connect(this, &Beam::otpReady, context, [this, context]() {
    Tau5Logger::instance().info( "BEAM restart complete");
    isRestarting = false;
    emit restartComplete();
    context->deleteLater();
  });
}