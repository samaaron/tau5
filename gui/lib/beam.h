#ifndef BEAM_H
#define BEAM_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>

class Beam : public QObject
{
  Q_OBJECT

public:
  explicit Beam(QObject *parent, const QString &basePath, const QString &appName, const QString &version, quint16 port, bool devMode);
  ~Beam();

  void startElixirServerDev();
  void startElixirServerProd();

signals:
  void standardOutput(const QString &output);
  void standardError(const QString &error);

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
  bool serverReady;

  void startProcess(const QString &cmd, const QStringList &args);
  bool isWindows() const;
  bool isMacOS() const;
  void killBeamProcess();
};

#endif // BEAM_H