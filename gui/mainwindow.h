#pragma once

#include <QMainWindow>
#include <QDateTime>
#include <memory>

// On windows, we need to include winsock2 before other instances of winsock
#ifdef WIN32
#include <winsock2.h>
#endif

class PhxWidget;
class DebugPane;
class ControlLayer;
class Beam;
class LoadingOverlay;

class MainWindow : public QMainWindow
{
  Q_OBJECT

signals:
  void allComponentsLoaded();

public:
  explicit MainWindow(bool devMode = false, QWidget *parent = nullptr);
  ~MainWindow();

  bool connectToServer(quint16 port);
  void setBeamInstance(Beam *beam);
  void toggleConsole();

public slots:
  void handleGuiLog(const QString &message, bool isError);
  void handleMainWindowLoaded();
  void handleLiveDashboardLoaded();
  void handleElixirConsoleLoaded();
  void handleWebDevToolsLoaded();

protected:
  void closeEvent(QCloseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void moveEvent(QMoveEvent *event) override;

private slots:
  void showAbout() const;
  void handleBeamOutput(const QString &output);
  void handleBeamError(const QString &error);
  void handleSizeDown();
  void handleSizeUp();
  void handleOpenExternalBrowser();
  void handleResetBrowser();
  void handleBeamRestart();

private:
  void initializePhxWidget(quint16 port);
  void initializeDebugPane();
  void initializeControlLayer();
  void checkAllComponentsLoaded();

private:
  std::unique_ptr<PhxWidget> phxWidget;
  std::unique_ptr<DebugPane> debugPane;
  std::unique_ptr<ControlLayer> controlLayer;
  std::unique_ptr<LoadingOverlay> loadingOverlay;
  QDateTime loadingOverlayStartTime;
  Beam *beamInstance;
  bool m_devMode;
  
  bool m_mainWindowLoaded;
  bool m_liveDashboardLoaded;
  bool m_elixirConsoleLoaded;
  bool m_webDevToolsLoaded;
  bool m_allComponentsSignalEmitted;
};