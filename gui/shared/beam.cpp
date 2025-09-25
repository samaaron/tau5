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
#include "cli_args.h"
#include "common.h"

using namespace Tau5Common;

Beam::Beam(QObject *parent, const Tau5CLI::ServerConfig& config, const QString &basePath, const QString &appName, const QString &version, quint16 port)
    : QObject(parent), appBasePath(basePath), process(new QProcess(this)),
      beamPid(0), heartbeatPort(0), serverReady(false), otpTreeReady(false),
      appName(appName), appVersion(version), isRestarting(false),
      m_config(&config)
{
  if (!Tau5Logger::isInitialized()) {
    qFatal("Beam: Tau5Logger must be initialized before creating Beam instances");
  }

  // Extract values from config
  const Tau5CLI::CommonArgs& args = config.getArgs();
  devMode = (args.env == Tau5CLI::CommonArgs::Env::Dev);
  enableMcp = args.mcp;
  enableRepl = args.repl;

  // Determine deployment mode from config
  std::string mode = config.getResolvedMode();
  if (mode == "node") {
    deploymentMode = DeploymentMode::Node;
  } else if (mode == "central") {
    deploymentMode = DeploymentMode::Central;
  } else {
    deploymentMode = DeploymentMode::Gui;
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
  heartbeatEnabled = true;
  startupTimer = nullptr;


  connect(process, &QProcess::readyReadStandardOutput,
          this, &Beam::handleStandardOutput);
  connect(process, &QProcess::readyReadStandardError,
          this, &Beam::handleStandardError);

  heartbeatTimer = new QTimer(this);
  QString intervalStr = qEnvironmentVariable("TAU5_HB_GUI_INTERVAL_MS");
  int interval = intervalStr.isEmpty() ? 5000 : intervalStr.toInt();
  if (interval < 1000) interval = 5000;
  heartbeatTimer->setInterval(interval);
  heartbeatTimer->setTimerType(Qt::CoarseTimer);
  connect(heartbeatTimer, &QTimer::timeout, this, &Beam::sendHeartbeat);
  
  Tau5Logger::instance().debug(QString("Heartbeat timer configured: interval=%1ms, single-shot=%2")
                                .arg(heartbeatTimer->interval())
                                .arg(heartbeatTimer->isSingleShot() ? "true" : "false"));

  if (devMode)
  {
    startElixirServerDev();
  }
  else
  {
    // In release mode, basePath should point directly to the server directory
    releaseRoot = QFileInfo(QString("%1/").arg(basePath)).absoluteFilePath();
    releaseSysPath = QFileInfo(QString("%1/releases/%2/sys").arg(basePath).arg(version)).absoluteFilePath();
    releaseStartPath = QFileInfo(QString("%1/releases/%2/start").arg(basePath).arg(version)).absoluteFilePath();
    releaseVmArgsPath = QFileInfo(QString("%1/releases/%2/vm.args").arg(basePath).arg(version)).absoluteFilePath();
    releaseLibPath = QFileInfo(QString("%1/lib").arg(basePath)).absoluteFilePath();
#if defined(Q_OS_WIN)
    releaseRoot = releaseRoot.replace("/", "\\");
    releaseSysPath = releaseSysPath.replace("/", "\\");
    releaseStartPath = releaseStartPath.replace("/", "\\");
    releaseVmArgsPath = releaseVmArgsPath.replace("/", "\\");
    releaseLibPath = releaseLibPath.replace("/", "\\");
#endif

    // In release mode, basePath already points to the server directory
    QDir releaseDir(basePath);
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
  if (startupTimer && startupTimer->isActive())
  {
    startupTimer->stop();
  }

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

  // Check for error messages first
  QRegularExpression serverErrorRegex("\\[TAU5_SERVER_ERROR:(.+)\\]");
  QRegularExpressionMatch serverErrorMatch = serverErrorRegex.match(outputStr);
  
  if (serverErrorMatch.hasMatch())
  {
    QString errorMessage = serverErrorMatch.captured(1);
    QString fullErrorMessage = QString("Server startup failed: %1").arg(errorMessage);
    
    // Log to Tau5Logger
    Tau5Logger::instance().error(fullErrorMessage);
    
    // CRITICAL: Print to stderr for headless mode visibility
    std::cerr << "\n" << fullErrorMessage.toStdString() << std::endl;
    
    // Emit signal for GUI mode
    emit standardError(fullErrorMessage);
    
    // Stop the startup timer
    if (startupTimer && startupTimer->isActive()) {
      startupTimer->stop();
    }
    
    // Exit with appropriate error code
    if (errorMessage.contains("port") && errorMessage.contains("in use")) {
      QCoreApplication::exit(static_cast<int>(ExitCode::PORT_IN_USE));
    } else if (errorMessage.contains("heartbeat")) {
      QCoreApplication::exit(static_cast<int>(ExitCode::HEARTBEAT_PORT_FAILED));
    } else {
      QCoreApplication::exit(static_cast<int>(ExitCode::BEAM_START_FAILED));
    }
    return;
  }
  
  QRegularExpression serverInfoRegex("\\[TAU5_SERVER_INFO:PID=(\\d+),HTTP_PORT=(\\d+),HEARTBEAT_PORT=(\\d+),MCP_PORT=(\\d+)\\]");
  QRegularExpressionMatch serverInfoMatch = serverInfoRegex.match(outputStr);
  
  if (serverInfoMatch.hasMatch())
  {
    beamPid = serverInfoMatch.captured(1).toLongLong();
    quint16 actualPort = serverInfoMatch.captured(2).toUInt();
    quint16 receivedHeartbeatPort = serverInfoMatch.captured(3).toUInt();
    quint16 receivedMcpPort = serverInfoMatch.captured(4).toUInt();

    Tau5Logger::instance().debug(QString("Captured server info - PID: %1, HTTP: %2, Heartbeat: %3, MCP: %4")
      .arg(beamPid).arg(actualPort).arg(receivedHeartbeatPort).arg(receivedMcpPort));

    if (actualPort > 0) {
      appPort = actualPort;
    }
    
    if (receivedHeartbeatPort > 0 && heartbeatEnabled) {
      heartbeatPort = receivedHeartbeatPort;
      Tau5Logger::instance().info(QString("Using BEAM-allocated heartbeat port: %1").arg(heartbeatPort));
    } else if (heartbeatEnabled && receivedHeartbeatPort == 0) {
      Tau5Logger::instance().error("FATAL: Heartbeat enabled but no port received from BEAM");
      emit standardError("Server failed to allocate heartbeat port");
      QCoreApplication::exit(static_cast<int>(ExitCode::HEARTBEAT_PORT_FAILED));
      return;
    }

    serverReady = true;
    
    // Stop the startup timer since we received server info
    if (startupTimer && startupTimer->isActive()) {
      startupTimer->stop();
    }
    
    if (heartbeatPort > 0 && heartbeatEnabled && heartbeatTimer && !heartbeatTimer->isActive()) {
      heartbeatTimer->start();
    }

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

QProcessEnvironment Beam::createControlledEnvironment(const Tau5CLI::ServerConfig& config)
{
    QProcessEnvironment env;

    // Get the args from the passed configuration
    const Tau5CLI::CommonArgs& args = config.getArgs();

    // System essentials - these are always needed
    env.insert("HOME", QDir::homePath());
    env.insert("LANG", "en_US.UTF-8");
    env.insert("LC_ALL", "en_US.UTF-8");
    env.insert("LC_CTYPE", "en_US.UTF-8");
    env.insert("TERM", "xterm-256color");

    // Temp directories
    env.insert("TMPDIR", QDir::tempPath());
    env.insert("TMP", QDir::tempPath());
    env.insert("TEMP", QDir::tempPath());

#ifdef Q_OS_UNIX
    // Get username from home directory path instead of environment
    QString homePath = QDir::homePath();
    QString userName = homePath.section('/', -1);
    env.insert("USER", userName);

    // DISPLAY is a legitimate pass-through for GUI applications
    if (qEnvironmentVariableIsSet("DISPLAY")) {
        env.insert("DISPLAY", qgetenv("DISPLAY"));
    }
#endif

    // PATH - dev builds need it for dev tools (git, etc.)
#ifndef TAU5_RELEASE_BUILD
    QProcessEnvironment sysEnv = QProcessEnvironment::systemEnvironment();
    QString systemPath = sysEnv.value("PATH");
    if (!systemPath.isEmpty()) {
        env.insert("PATH", systemPath);
    }
#endif

    // Apply all Tau5-specific variables from the config's environment map
    std::map<std::string, std::string> envVars = config.generateEnvironmentVars();
    for (const auto& [key, value] : envVars) {
        env.insert(QString::fromStdString(key), QString::fromStdString(value));
    }

    return env;
}

void Beam::startElixirServerDev()
{
  Tau5Logger::instance().info( "Starting Elixir server in Development mode");

  // Use controlled environment instead of system environment
  QProcessEnvironment env = createControlledEnvironment(*m_config);


  env.insert("TAU5_USE_STDIN_CONFIG", "true");
  env.insert("TAU5_HEARTBEAT_ENABLED", "true");

  if (appPort > 0) {
    env.insert("TAU5_LOCAL_PORT", QString::number(appPort));
  }

  // Heartbeat port is now handled in createControlledEnvironment()

  env.insert("PHX_HOST", "127.0.0.1");
  if (env.value("MIX_ENV").isEmpty()) {
    env.insert("MIX_ENV", "dev");
  }
  env.insert("RELEASE_DISTRIBUTION", "none");

  QString sessionPath = Tau5Logger::instance().currentSessionPath();
  env.insert("TAU5_LOG_DIR", sessionPath);
  Tau5Logger::instance().debug(QString("Setting TAU5_LOG_DIR to: %1").arg(sessionPath));

  if (env.value("TAU5_MCP_ENABLED") == "true") {
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
    Tau5Logger::instance().error("Please use --dev-server-path argument or set TAU5_SERVER_PATH environment variable");
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
  QProcessEnvironment env = createControlledEnvironment(*m_config);

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

  env.insert("TAU5_USE_STDIN_CONFIG", "true");
  env.insert("TAU5_HEARTBEAT_ENABLED", "true");
  env.insert("PHX_SERVER", "1");

  if (appPort > 0) {
    env.insert("TAU5_LOCAL_PORT", QString::number(appPort));
  }

  // Heartbeat port is now handled in createControlledEnvironment()

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

  if (env.value("TAU5_MCP_ENABLED") == "true") {
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
  if (!process) {
    Tau5Logger::instance().error("FATAL: Cannot write secrets - process not started");
    QCoreApplication::exit(static_cast<int>(ExitCode::STDIN_CONFIG_FAILED));
    return;
  }

  Tau5Logger::instance().debug("Writing secure configuration to process stdin");

  QString config;
  config += sessionToken + "\n";
  config += heartbeatToken + "\n";
  config += secretKeyBase + "\n";
  // Ports are now allocated by BEAM, not sent via stdin

  process->write(config.toUtf8());
  process->closeWriteChannel();

  Tau5Logger::instance().debug(QString("Secure configuration written (%1 bytes) and stdin closed")
                              .arg(config.toUtf8().size()));
}

void Beam::startProcess(const QString &cmd, const QStringList &args)
{
  Tau5Logger::instance().debug( QString("Server process working directory: %1").arg(process->workingDirectory()));
  Tau5Logger::instance().debug( QString("Starting process: %1 %2").arg(cmd).arg(args.join(" ")));
  
  // Initialize and start the startup timeout timer (30 seconds)
  startupTimer = new QTimer(this);
  startupTimer->setSingleShot(true);
  startupTimer->setInterval(30000); // 30 seconds timeout
  connect(startupTimer, &QTimer::timeout, this, &Beam::handleStartupTimeout);
  startupTimer->start();
  
  Tau5Logger::instance().debug("Started startup timeout timer (30 seconds)");

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

#ifdef Q_OS_UNIX
  // For GUI builds: MUST prevent QWebEngine/Chrome file descriptors from being inherited
  // This requires Qt 6.6+ for the CloseFileDescriptors flag
  // Without this, Chrome FDs leak into BEAM causing issues
  #if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0) && !defined(BUILD_NODE_ONLY)
    QProcess::UnixProcessParameters params;
    params.flags = QProcess::UnixProcessFlag::UseVFork |           // Avoid copying WebEngine memory
                   QProcess::UnixProcessFlag::CloseFileDescriptors; // Prevent Chrome FD inheritance
    process->setUnixProcessParameters(params);
    Tau5Logger::instance().debug("Using vfork with FD isolation to prevent Chrome descriptor inheritance");
  #else
    // Node-only builds or Qt < 6.6
    // Node builds don't have WebEngine FDs to worry about
    Tau5Logger::instance().debug("Using standard fork for process creation");
  #endif
#endif
  // Note: Windows CreateProcess() doesn't need special handling - it already
  // behaves like spawn and doesn't copy the parent's memory
  process->start(cmd, args);

  if (!process->waitForStarted(5000))
  {
    QString errorMsg = QString("Error starting BEAM: %1\nCommand: %2\nArgs: %3")
                      .arg(process->errorString())
                      .arg(cmd)
                      .arg(args.join(" "));
    Tau5Logger::instance().error( errorMsg);
    emit standardError(errorMsg);
  } else {
    // Always write secrets via stdin - no fallback
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

void Beam::handleStartupTimeout()
{
  Tau5Logger::instance().error("Server startup timeout - no TAU5_SERVER_INFO received within 30 seconds");
  emit standardError("Server startup timeout - no response from server within 30 seconds");
  
  // Kill the process if it's still running
  if (process && process->state() != QProcess::NotRunning) {
    process->terminate();
    if (!process->waitForFinished(5000)) {
      process->kill();
    }
  }
  
  QCoreApplication::exit(static_cast<int>(ExitCode::BEAM_START_FAILED));
}

void Beam::sendHeartbeat()
{
  static int heartbeatCount = 0;
  heartbeatCount++;
  
  if (!serverReady) {
    Tau5Logger::instance().debug(QString("Heartbeat #%1 skipped - server not ready").arg(heartbeatCount));
    return;
  }
  
  if (!process || process->state() != QProcess::Running) {
    Tau5Logger::instance().debug(QString("Heartbeat #%1 skipped - process not running").arg(heartbeatCount));
    return;
  }

  if (!heartbeatSocket)
  {
    Tau5Logger::instance().warning(QString("Heartbeat #%1 - Cannot send - UDP socket not created").arg(heartbeatCount));
    return;
  }
  
  if (heartbeatPort == 0)
  {
    Tau5Logger::instance().warning(QString("Heartbeat #%1 - Cannot send - port not yet allocated").arg(heartbeatCount));
    return;
  }

  QString heartbeatMsg = QString("HEARTBEAT:%1\n").arg(heartbeatToken);
  QByteArray datagram = heartbeatMsg.toUtf8();

  qint64 sent = heartbeatSocket->writeDatagram(datagram, QHostAddress::LocalHost, heartbeatPort);

  if (sent == -1) {
    Tau5Logger::instance().warning(QString("Heartbeat #%1 - Failed to send UDP: %2")
                                  .arg(heartbeatCount)
                                  .arg(heartbeatSocket->errorString()));
  } else {
    if (heartbeatCount <= 10 || heartbeatCount % 10 == 0) {
      Tau5Logger::instance().debug(QString("Heartbeat #%1 sent successfully to port %2 (bytes: %3)")
                                  .arg(heartbeatCount)
                                  .arg(heartbeatPort)
                                  .arg(sent));
    }
  }
  
  if (heartbeatTimer && !heartbeatTimer->isActive()) {
    Tau5Logger::instance().error(QString("CRITICAL: Timer stopped after heartbeat #%1!").arg(heartbeatCount));
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
  // Use synchronous QProcess::execute for reliable kill during shutdown
  // This ensures the kill commands complete before the GUI process exits
  Tau5Logger::instance().debug(QString("Unix: Sending SIGTERM to PID: %1").arg(beamPid));
  int result = QProcess::execute("kill", {"-TERM", QString::number(beamPid)});

  if (result != 0)
  {
    Tau5Logger::instance().debug(QString("Process %1 not found or already terminated").arg(beamPid));
    beamPid = 0;
    return;
  }

  for (int i = 5; i > 0; --i)
  {
    result = QProcess::execute("kill", {"-0", QString::number(beamPid)});

    if (result != 0)
    {
      Tau5Logger::instance().debug(QString("Process %1 terminated gracefully").arg(beamPid));
      beamPid = 0;
      return;
    }

    Tau5Logger::instance().debug(QString("Process %1 still running, waiting... %2").arg(beamPid).arg(i));
    QThread::msleep(1000);
  }

  Tau5Logger::instance().debug(QString("Unix: Sending SIGKILL to PID: %1").arg(beamPid));
  QProcess::execute("kill", {"-9", QString::number(beamPid)});
  beamPid = 0;
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

  // During restart, reuse all existing tokens so the GUI doesn't need to reload
  // The tokens are already set from the initial startup, no need to regenerate
  Tau5Logger::instance().debug("Reusing existing secure tokens for restart");

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