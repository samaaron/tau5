#pragma once

#include <QMainWindow>
#include <QDateTime>
#include <memory>

// On windows, we need to include winsock2 before other instances of winsock
#ifdef WIN32
#include <winsock2.h>
#endif

class MainPhxWidget;
#ifdef BUILD_WITH_DEBUG_PANE
class DebugPane;
#endif
class ControlLayer;
class Beam;
class ConsoleOverlay;
class TransitionOverlay;

#ifndef Q_OS_MACOS
class CustomTitleBar;
namespace QWK {
    class WidgetWindowAgent;
}
#endif

class MainWindow : public QMainWindow
{
  Q_OBJECT

signals:
  void allComponentsLoaded();

public:
  explicit MainWindow(bool devMode = false, bool enableDebugPane = true, bool enableMcp = false, bool enableRepl = false, bool allowRemoteAccess = false, int channel = 0, QWidget *parent = nullptr);
  ~MainWindow();

  bool connectToServer(quint16 port);
  void setBeamInstance(Beam *beam);
  void toggleConsole();

public slots:
  void handleBootLog(const QString &message, bool isError = false);
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
  void startTransitionToApp();
  void onFadeToBlackComplete();
  void onAppPageReady();

private:
  void initializeDebugPane();
  void initializeControlLayer();
  void initializeWebViewConnections();
  void checkAllComponentsLoaded();
  bool isElixirReplEnabled();

private:
  std::unique_ptr<MainPhxWidget> phxWidget;
#ifdef BUILD_WITH_DEBUG_PANE
  std::unique_ptr<DebugPane> debugPane;
#endif
  std::unique_ptr<ControlLayer> controlLayer;
  std::unique_ptr<ConsoleOverlay> consoleOverlay;
  std::unique_ptr<TransitionOverlay> transitionOverlay;
#ifndef Q_OS_MACOS
  CustomTitleBar *m_titleBar;
  QWK::WidgetWindowAgent *m_windowAgent;
#endif
  QDateTime bootStartTime;
  Beam *beamInstance;
  bool m_devMode;
  bool m_enableDebugPane;
  bool m_enableMcp;
  bool m_enableRepl;
  bool m_allowRemoteAccess;
  quint16 m_serverPort;
  
  bool m_mainWindowLoaded;
  bool m_liveDashboardLoaded;
  bool m_elixirConsoleLoaded;
  bool m_webDevToolsLoaded;
  bool m_allComponentsSignalEmitted;
  bool m_beamReady;
  bool m_debugPaneShouldBeVisible = false;
  int m_channel;

  static constexpr int DEBUG_PANE_RESTORE_DELAY_MS = 500;
};