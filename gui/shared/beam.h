#ifndef BEAM_H
#define BEAM_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>
#include <QUdpSocket>

class Beam : public QObject
{
  Q_OBJECT

public:
  explicit Beam(QObject *parent, const QString &basePath, const QString &appName, const QString &version, quint16 port, bool devMode, bool enableMcp = false, bool enableRepl = false);
  ~Beam();
  
  QString getSessionToken() const { return sessionToken; }
  quint16 getPort() const { return appPort; }

  void startElixirServerDev();
  void startElixirServerProd();
  void restart();

signals:
  void standardOutput(const QString &output);
  void standardError(const QString &error);
  void otpReady();
  void restartComplete();

private slots:
  void handleStandardOutput();
  void handleStandardError();
  void sendHeartbeat();

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
  QUdpSocket *heartbeatSocket;
  quint16 heartbeatPort;
  QString heartbeatToken;
  bool serverReady;
  bool otpTreeReady;
  QString sessionToken;
  bool devMode;
  QString appName;
  QString appVersion;
  bool isRestarting;
  bool enableMcp;
  bool enableRepl;
  bool useStdinConfig;
  QString secretKeyBase;

  void startProcess(const QString &cmd, const QStringList &args);
  void writeSecretsToStdin();
  bool isWindows() const;
  bool isMacOS() const;
  void killBeamProcess();
  void continueRestart();
  void checkPortAndStartNewProcess();
  void startNewBeamProcess();
};

#endif // BEAM_H