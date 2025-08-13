#ifndef DEBUGPANE_H
#define DEBUGPANE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QTabWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <memory>
#include <QList>

class QPushButton;
class LogWidget;
class LogFileManager;
class QLabel;
class QPropertyAnimation;
class QTextEdit;
class ConsoleWidget;
class PhxWebView;
class SandboxedWebView;
class QShortcut;

class DebugPane : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(int slidePosition READ slidePosition WRITE setSlidePosition)

public:
  enum ViewMode
  {
    BeamLogOnly,
    DevToolsOnly,
    SideBySide
  };

  explicit DebugPane(QWidget *parent = nullptr);
  ~DebugPane();

  void appendOutput(const QString &text, bool isError = false);
  void appendGuiLog(const QString &text, bool isError = false);
  void toggle();
  void slide(bool show);
  bool isVisible() const { return m_isVisible; }
  void setWebView(PhxWebView *webView);
  void setViewMode(ViewMode mode);
  void setLiveDashboardUrl(const QString &url);
  void setElixirConsoleUrl(const QString &url);
  void saveSettings();
  void restoreSettings();
  void setRestartButtonEnabled(bool enabled);
  void resetDevPaneBrowsers();

  int slidePosition() const { return pos().y(); }
  void setSlidePosition(int pos) { move(x(), pos); }

signals:
  void visibilityChanged(bool visible);
  void liveDashboardLoaded();
  void elixirConsoleLoaded();
  void webDevToolsLoaded();
  void restartBeamRequested();

private slots:
  void animationFinished();
  void showBeamLogOnly();
  void showDevToolsOnly();
  void showSideBySide();
  void showBeamLog();
  void showGuiLog();
  void showElixirConsole();
  void showTau5MCPLog();
  void showGuiMCPLog();
  void showDevToolsTab();
  void showLiveDashboardTab();
  void handleInspectElementRequested();

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void setupUi();
  void setupViewControls();
  void setupConsole();
  void setupDevTools();
  void updateViewMode();
  int constrainHeight(int requestedHeight) const;
  void switchConsoleTab(int index, const QList<QPushButton *> &tabButtons);
  void switchDevToolsTab(int index);

private:
  QVBoxLayout *m_mainLayout;
  QWidget *m_headerWidget;
  QHBoxLayout *m_headerLayout;
  QSplitter *m_splitter;

  QWidget *m_consoleContainer;
  QStackedWidget *m_consoleStack;
  QPushButton *m_beamLogTabButton;
  QPushButton *m_guiLogTabButton;
  QPushButton *m_elixirConsoleTabButton;
  QPushButton *m_tau5MCPTabButton;
  QPushButton *m_guiMCPTabButton;
  QWidget *m_elixirConsoleContainer;
  SandboxedWebView *m_elixirConsoleView;
  QTabWidget *m_devToolsTabs;
  QWidget *m_devToolsMainContainer;
  QStackedWidget *m_devToolsStack;
  QPushButton *m_devToolsTabButton;
  QPushButton *m_liveDashboardTabButton;
  QWidget *m_devToolsContainer;
  SandboxedWebView *m_devToolsView;
  QWidget *m_liveDashboardContainer;
  SandboxedWebView *m_liveDashboardView;

  PhxWebView *m_targetWebView;
  int m_currentFontSize;
  int m_guiLogFontSize;

  QPushButton *m_beamLogButton;
  QPushButton *m_devToolsButton;
  QPushButton *m_sideBySideButton;
  QPushButton *m_restartButton;
  QPushButton *m_resetButton;
  QPushButton *m_closeButton;

  std::unique_ptr<QPropertyAnimation> m_slideAnimation;
  bool m_isVisible;
  int m_maxLines;
  ViewMode m_currentMode;

  bool m_isResizing;
  int m_resizeStartY;
  int m_resizeStartHeight;
  bool m_isHoveringHandle;

  QWidget *m_dragHandleWidget;
  QTimer *m_dragHandleAnimationTimer;
  QWidget *m_animationBar;
  QLabel *m_restartLabel;

  // NEW: LogWidget instances for cleaner implementation
  LogWidget *m_newBeamLogWidget;
  LogWidget *m_newGuiLogWidget;
  LogWidget *m_newTau5MCPWidget;
  LogWidget *m_newGuiMCPWidget;

  // Store original URLs for reset functionality
  QString m_liveDashboardUrl;
  QString m_elixirConsoleUrl;
  
  // Log file manager for GUI logs
  LogFileManager *m_guiLogFileManager;

public:
  static constexpr int RESIZE_HANDLE_HEIGHT = 10;       // Interaction area
  static constexpr int RESIZE_HANDLE_VISUAL_HEIGHT = 4; // Visual line height
};

#endif // DEBUGPANE_H