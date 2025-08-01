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
class SearchFunctionality;
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
  bool isVisible() const { return m_isVisible; }
  void setWebView(PhxWebView *webView);
  void setViewMode(ViewMode mode);
  void setLiveDashboardUrl(const QString &url);
  void setElixirConsoleUrl(const QString &url);
  void saveSettings();
  void restoreSettings();
  void setRestartButtonEnabled(bool enabled);

  int slidePosition() const { return pos().y(); }
  void setSlidePosition(int pos) { move(x(), pos); }

signals:
  void visibilityChanged(bool visible);
  void liveDashboardLoaded();
  void elixirConsoleLoaded();
  void webDevToolsLoaded();
  void restartBeamRequested();

private slots:
  void handleAutoScrollToggled(bool checked);
  void animationFinished();
  void showBeamLogOnly();
  void showDevToolsOnly();
  void showSideBySide();
  void handleZoomIn();
  void handleZoomOut();
  void handleConsoleZoomIn();
  void handleConsoleZoomOut();
  void handleGuiLogZoomIn();
  void handleGuiLogZoomOut();
  void handleElixirConsoleZoomIn();
  void handleElixirConsoleZoomOut();
  void showBeamLog();
  void showGuiLog();
  void showElixirConsole();
  void showDevToolsTab();
  void showLiveDashboardTab();
  void findNext();
  void findPrevious();
  void toggleSearchBar();
  void performSearch();
  void handleSearchShortcut();
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
  void slide(bool show);
  void updateViewMode();
  int constrainHeight(int requestedHeight) const;
  void switchConsoleTab(int index, const QList<QPushButton *> &tabButtons);
  void switchDevToolsTab(int index);
  void getCurrentSearchContext(QWidget *&searchWidget, QLineEdit *&searchInput, QTextEdit *&textEdit, QString *&lastSearchText);
  void highlightAllMatches(QTextEdit *textEdit, const QString &searchText, const QTextCursor &currentMatch);
  void setupShortcuts();
  void cleanupShortcuts();

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
  QWidget *m_beamLogContainer;
  QVBoxLayout *m_beamLogLayout;
  QTextEdit *m_outputDisplay;
  QPushButton *m_autoScrollButton;
  QPushButton *m_beamLogSearchButton;
  QPushButton *m_consoleZoomInButton;
  QPushButton *m_consoleZoomOutButton;
  QPushButton *m_devToolsZoomInButton;
  QPushButton *m_devToolsZoomOutButton;
  QWidget *m_guiLogContainer;
  QVBoxLayout *m_guiLogLayout;
  QTextEdit *m_guiLogDisplay;
  QPushButton *m_guiLogAutoScrollButton;
  QPushButton *m_guiLogSearchButton;
  QPushButton *m_guiLogZoomInButton;
  QPushButton *m_guiLogZoomOutButton;
  QWidget *m_elixirConsoleContainer;
  SandboxedWebView *m_elixirConsoleView;
  QPushButton *m_elixirConsoleZoomInButton;
  QPushButton *m_elixirConsoleZoomOutButton;
  QTabWidget *m_devToolsTabs;
  QWidget *m_devToolsMainContainer;
  QStackedWidget *m_devToolsStack;
  QPushButton *m_devToolsTabButton;
  QPushButton *m_liveDashboardTabButton;
  QWidget *m_devToolsContainer;
  SandboxedWebView *m_devToolsView;
  QPushButton *m_zoomInButton;
  QPushButton *m_zoomOutButton;
  QWidget *m_liveDashboardContainer;
  SandboxedWebView *m_liveDashboardView;
  QPushButton *m_liveDashboardZoomInButton;
  QPushButton *m_liveDashboardZoomOutButton;

  PhxWebView *m_targetWebView;
  int m_currentFontSize;
  int m_guiLogFontSize;

  QPushButton *m_beamLogButton;
  QPushButton *m_devToolsButton;
  QPushButton *m_sideBySideButton;
  QPushButton *m_restartButton;
  QPushButton *m_closeButton;
  QTimer *m_restartAnimationTimer;

  std::unique_ptr<QPropertyAnimation> m_slideAnimation;
  bool m_isVisible;
  bool m_autoScroll;
  bool m_guiLogAutoScroll;
  int m_maxLines;
  ViewMode m_currentMode;

  bool m_isResizing;
  int m_resizeStartY;
  int m_resizeStartHeight;
  bool m_isHoveringHandle;

  QWidget *m_dragHandleWidget;

  // Search functionality
  QWidget *m_beamLogSearchWidget;
  QLineEdit *m_beamLogSearchInput;
  QPushButton *m_beamLogSearchCloseButton;
  QString m_beamLogLastSearchText;

  QWidget *m_guiLogSearchWidget;
  QLineEdit *m_guiLogSearchInput;
  QPushButton *m_guiLogSearchCloseButton;
  QString m_guiLogLastSearchText;
  SearchFunctionality *m_searchFunctionality;

  // Shortcut management
  QList<QShortcut *> m_shortcuts;

public:
  static constexpr int RESIZE_HANDLE_HEIGHT = 10;       // Interaction area
  static constexpr int RESIZE_HANDLE_VISUAL_HEIGHT = 4; // Visual line height
};

#endif // DEBUGPANE_H