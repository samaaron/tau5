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
#include "../logger.h"

Beam::Beam(QObject *parent, const QString &basePath, const QString &appName, const QString &version, quint16 port, bool devMode)
    : QObject(parent), appBasePath(basePath), process(new QProcess(this)), 
      beamPid(0), serverReady(false), otpTreeReady(false), devMode(devMode),
      appName(appName), appVersion(version), isRestarting(false)
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

  if (process)
  {
    if (process->state() == QProcess::Running)
    {
      Logger::log(Logger::Info, "Attempting to terminate BEAM process...");
      process->terminate();
      if (!process->waitForFinished(5000))
      {
        Logger::log(Logger::Warning, "Process did not terminate gracefully, killing it...");
        process->kill();
        process->waitForFinished();
      }
    }
  }
  
  if (beamPid > 0)
  {
    killBeamProcess();
  }
}

void Beam::handleStandardOutput()
{
  QByteArray output = process->readAllStandardOutput();
  QString outputStr = QString::fromUtf8(output);
  
  // Check for BEAM PID marker
  QRegularExpression pidRegex("\\[TAU5_BEAM_PID:(\\d+)\\]");
  QRegularExpressionMatch pidMatch = pidRegex.match(outputStr);
  if (pidMatch.hasMatch())
  {
    beamPid = pidMatch.captured(1).toLongLong();
    Logger::log(Logger::Debug, QString("Captured BEAM PID: %1").arg(beamPid));
    serverReady = true;
    heartbeatTimer->start();
  }
  
  // Check for OTP ready marker
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
  env.insert("TAU5_ENV", "dev");
  env.insert("TAU5_SESSION_TOKEN", sessionToken);
  env.insert("TAU5_HEARTBEAT_ENABLED", "true");
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
  Logger::log(Logger::Info, "Starting Elixir server in Production mode");

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("TAU5_ENV", "prod");
  env.insert("TAU5_SESSION_TOKEN", sessionToken);
  env.insert("TAU5_HEARTBEAT_ENABLED", "true");
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
  
  for (int i = 5; i > 0; --i)
  {
    QProcess checkProcess;
    checkProcess.start("tasklist", {"/FI", QString("PID eq %1").arg(beamPid)});
    checkProcess.waitForFinished();
    QString output = checkProcess.readAllStandardOutput();
    
    if (!output.contains(QString::number(beamPid)))
    {
      qDebug() << "Process" << beamPid << "terminated gracefully";
      return;
    }
    
    qDebug() << "Process" << beamPid << "still running, waiting..." << i;
    QThread::msleep(1000);
  }
  
  qDebug() << "Windows: Force killing PID:" << beamPid;
  
  // Use QProcess instance to suppress error output
  QProcess forceKill;
  forceKill.start("taskkill", {"/F", "/PID", QString::number(beamPid)});
  forceKill.waitForFinished();
  
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

  QThread::msleep(1000);
  
#ifdef Q_OS_WIN
  QProcess finalCheck;
  finalCheck.start("tasklist", {"/FI", QString("PID eq %1").arg(beamPid)});
  finalCheck.waitForFinished();
  QString finalOutput = finalCheck.readAllStandardOutput();
  
  if (finalOutput.contains(QString::number(beamPid)))
  {
    qDebug() << "WARNING: Unable to terminate process" << beamPid;
  }
  else
  {
    qDebug() << "Process" << beamPid << "successfully terminated";
  }
#else
  int finalResult = QProcess::execute("kill", {"-0", QString::number(beamPid)});
  
  if (finalResult == 0)
  {
    qDebug() << "WARNING: Unable to terminate process" << beamPid;
  }
  else
  {
    qDebug() << "Process" << beamPid << "successfully terminated";
  }
#endif
}

void Beam::restart()
{
  Logger::log(Logger::Info, "Restarting BEAM process...");
  
  // Prevent multiple restarts
  if (isRestarting)
  {
    Logger::log(Logger::Warning, "Restart already in progress");
    return;
  }
  isRestarting = true;
  
  // Stop the heartbeat timer
  if (heartbeatTimer && heartbeatTimer->isActive())
  {
    heartbeatTimer->stop();
  }
  
  // Reset state flags
  serverReady = false;
  otpTreeReady = false;
  
  // Terminate the current process
  if (process && process->state() == QProcess::Running)
  {
    Logger::log(Logger::Info, "Terminating current BEAM process...");
    
    // Disconnect existing process signals
    disconnect(process, &QProcess::readyReadStandardOutput, this, &Beam::handleStandardOutput);
    disconnect(process, &QProcess::readyReadStandardError, this, &Beam::handleStandardError);
    
    // Connect to finished signal - using a direct connection
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Beam::continueRestart);
    
    process->terminate();
    
    // Set up a timer to force kill if it doesn't terminate gracefully
    QTimer::singleShot(5000, this, [this]() {
      if (process && process->state() == QProcess::Running)
      {
        Logger::log(Logger::Warning, "Process did not terminate gracefully, killing it...");
        process->kill();
        
        // Wait a bit more, then force continue
        QTimer::singleShot(2000, this, [this]() {
          if (process && process->state() == QProcess::Running)
          {
            Logger::log(Logger::Error, "Failed to kill process, forcing restart");
          }
          continueRestart();
        });
      }
    });
  }
  else
  {
    // Process not running, continue immediately
    continueRestart();
  }
}

void Beam::continueRestart()
{
  // Prevent multiple calls
  if (!isRestarting)
  {
    Logger::log(Logger::Warning, "continueRestart called but not restarting");
    return;
  }
  
  Logger::log(Logger::Info, "Continuing BEAM restart...");
  
  // Disconnect the finished signal to prevent multiple calls
  if (process)
  {
    disconnect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
               this, &Beam::continueRestart);
  }
  
  // Kill the BEAM process by PID if it's still running
  if (beamPid > 0)
  {
    killBeamProcess();
    beamPid = 0;
  }
  
  // Delete the old process
  if (process)
  {
    process->deleteLater();
    process = nullptr;
  }
  
  // Start checking if the port is available
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
  
  // Check if the port is available
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
  
  // Create a new process
  process = new QProcess(this);
  
  // Reconnect signals
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
  
  // Generate a new session token
  sessionToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
  Logger::log(Logger::Debug, QString("Generated new session token: %1").arg(sessionToken));
  
  // Start the new process
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