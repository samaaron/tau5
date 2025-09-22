#ifndef BEAM_H
#define BEAM_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>
#include <QUdpSocket>

namespace Tau5CLI {
    class ServerConfig;
}

class Beam : public QObject
{
  Q_OBJECT

public:
  enum class DeploymentMode {
    Gui,      // Running as Qt GUI desktop app
    Node,     // Running as standalone node server (headless)
    Central   // Running as the authoritative tau5.sonic-pi.net server
  };

  explicit Beam(QObject *parent, const Tau5CLI::ServerConfig& config, const QString &basePath, const QString &appName, const QString &version, quint16 port);
  ~Beam();
  
  QString getSessionToken() const { return sessionToken; }
  quint16 getPort() const { return appPort; }
  qint64 getBeamPid() const { return beamPid; }

  void startElixirServerDev();
  void startElixirServerProd();
  void restart();

signals:
  void standardOutput(const QString &output);
  void standardError(const QString &error);
  void otpReady();
  void restartComplete();
  void actualPortAllocated(quint16 port);

private slots:
  void handleStandardOutput();
  void handleStandardError();
  void sendHeartbeat();
  void handleStartupTimeout();

private:
  quint16 appPort;
  QString appBasePath;
  QString releaseRoot;
  QString releasePath;
  QString releaseSysPath;
  QString releaseStartPath;
  QString releaseVmArgsPath;
  QString releaseLibPath;
  QString releaseErlBinPath;
  QProcess *process;
  qint64 beamPid;
  QTimer *heartbeatTimer;
  QTimer *startupTimer;
  QUdpSocket *heartbeatSocket;
  quint16 heartbeatPort;
  QString heartbeatToken;
  bool serverReady;
  bool otpTreeReady;
  QString sessionToken;
  bool devMode;
  bool heartbeatEnabled;
  QString appName;
  QString appVersion;
  bool isRestarting;
  bool enableMcp;
  bool enableRepl;
  QString secretKeyBase;
  DeploymentMode deploymentMode;
  const Tau5CLI::ServerConfig* m_config;

  void startProcess(const QString &cmd, const QStringList &args);
  void writeSecretsToStdin();
  bool isWindows() const;
  bool isMacOS() const;
  void killBeamProcess();
  void continueRestart();
  void checkPortAndStartNewProcess();
  void startNewBeamProcess();
  QProcessEnvironment createControlledEnvironment(const Tau5CLI::ServerConfig& config);
};

#endif // BEAM_H