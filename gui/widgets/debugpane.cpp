#include "debugpane.h"
#include "logwidget.h"
#include "debugwidget.h"
#include "debugpane/customsplitter.h"
#include "debugpane/buttonutilities.h"
#include "debugpane/activitytabbutton.h"
#include "debugpane/themestyles.h"
#include "debugpane/zoomcontrol.h"
#include "debugpane/tabswitcher.h"
#include "debugpane/animationcontrol.h"
#include "../styles/StyleManager.h"
#include <QFontDatabase>
#include "phxwebview.h"
#include "sandboxedwebview.h"
#include "../shared/tau5logger.h"
#include "../lib/fontloader.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QShortcut>
#include <QTextEdit>
#include <QPushButton>
#include <QPointer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QGraphicsColorizeEffect>
#include <QPainter>
#include <QPainterPath>
#include <QSplitter>
#include <QTabWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QDateTime>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <QEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QCursor>
#include <QClipboard>
#include <QGuiApplication>
#include <QMainWindow>
#include <QIcon>
#include <QPixmap>
#include <QSvgRenderer>
#include <QByteArray>
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <QRegularExpression>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QFont>
#include <QSplitterHandle>
#include <QKeyEvent>
#include <QTextDocument>
#include <QTextEdit>
#include <QRandomGenerator>
#include <cmath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

static QPushButton *createCodiconButton(QWidget *parent, const QChar &icon, const QString &tooltip,
                                        bool checkable = false, bool checked = false)
{
  QPushButton *button = new QPushButton(icon, parent);
  button->setToolTip(tooltip);

  QString fontFamily = (icon == '+' || icon == '-') ? "Segoe UI, Arial" : "codicon";
  QString fontSize = (icon == '+' || icon == '-') ? "16px" : "14px";

  button->setStyleSheet(QString(
                            "QPushButton {"
                            "  font-family: '%1';"
                            "  font-size: %2;"
                            "  font-weight: bold;"
                            "  color: %3;"
                            "  background: transparent;"
                            "  border: none;"
                            "  padding: 2px;"
                            "}"
                            "QPushButton:hover {"
                            "  color: %4;"
                            "  background-color: %6;"
                            "  border-radius: 3px;"
                            "}"
                            "QPushButton:pressed {"
                            "  background-color: %7;"
                            "}"
                            "QPushButton:checked {"
                            "  color: %5;"
                            "  background-color: %8;"
                            "}")
                            .arg(fontFamily)
                            .arg(fontSize)
                            .arg(StyleManager::Colors::ACCENT_PRIMARY)
                            .arg(StyleManager::Colors::TEXT_PRIMARY)
                            .arg(StyleManager::Colors::STATUS_ERROR)
                            .arg(StyleManager::Colors::textPrimaryAlpha(25))
                            .arg(StyleManager::Colors::textPrimaryAlpha(51))
                            .arg(StyleManager::Colors::accentPrimaryAlpha(51)));

  if (checkable)
  {
    button->setCheckable(true);
    button->setChecked(checked);
  }

  button->setFixedSize(24, 24);
  return button;
}

DebugPane::DebugPane(QWidget *parent, bool devMode, bool enableMcp, bool enableRepl)
    : QWidget(parent), m_isVisible(false),
      m_maxLines(5000), m_currentMode(BeamLogOnly), m_isResizing(false),
      m_resizeStartY(0), m_resizeStartHeight(0), m_isHoveringHandle(false),
      m_targetWebView(nullptr), m_devToolsView(nullptr), m_liveDashboardView(nullptr),
      m_elixirConsoleView(nullptr),
      m_currentFontSize(12), m_guiLogFontSize(12),
      m_devToolsMainContainer(nullptr),
      m_devToolsStack(nullptr), m_devToolsTabButton(nullptr), m_liveDashboardTabButton(nullptr),
      m_dragHandleWidget(nullptr), m_dragHandleAnimationTimer(nullptr), m_animationBar(nullptr),
      m_restartLabel(nullptr), m_restartButton(nullptr), m_resetButton(nullptr), m_closeButton(nullptr),
      m_newBootLogWidget(nullptr), m_newBeamLogWidget(nullptr), m_newGuiLogWidget(nullptr), m_newTau5MCPWidget(nullptr),
      m_newTidewaveMCPWidget(nullptr), m_newGuiMCPWidget(nullptr), m_consoleToolbarStack(nullptr),
      m_bootLogTabButton(nullptr), m_elixirConsoleTabButton(nullptr), m_tau5MCPTabButton(nullptr), m_tidewaveMCPTabButton(nullptr), m_guiMCPTabButton(nullptr),
      m_devMode(devMode), m_mcpEnabled(enableMcp), m_replEnabled(enableRepl)
{
  static bool codiconLoaded = false;
  if (!codiconLoaded)
  {
    int fontId = QFontDatabase::addApplicationFont(":/fonts/codicon.ttf");
    if (fontId != -1)
    {
      Tau5Logger::instance().debug("[DebugPane] Loaded codicon font");
      codiconLoaded = true;
    }
  }

  setAttribute(Qt::WA_TranslucentBackground);
  setWindowFlags(Qt::FramelessWindowHint);
  setMouseTracking(true);
  setMinimumHeight(100);

  setupUi();
  hide();
}

DebugPane::~DebugPane()
{
  if (m_dragHandleAnimationTimer)
  {
    m_dragHandleAnimationTimer->stop();
    m_dragHandleAnimationTimer->deleteLater();
    m_dragHandleAnimationTimer = nullptr;
  }
}

bool DebugPane::isElixirReplEnabled()
{
  // Check both the member variable and the environment variable for consistency
  return m_replEnabled || (qgetenv("TAU5_ELIXIR_REPL_ENABLED") == "true");
}

bool DebugPane::isMcpEnabled()
{
  return m_mcpEnabled;
}

void DebugPane::setupUi()
{
  setFocusPolicy(Qt::StrongFocus);

  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  m_mainLayout->setSpacing(0);

  m_dragHandleWidget = new QWidget(this);
  m_dragHandleWidget->setFixedHeight(RESIZE_HANDLE_VISUAL_HEIGHT);
  m_dragHandleWidget->setMouseTracking(true);
  m_dragHandleWidget->hide();
  m_dragHandleWidget->setStyleSheet(QString(
                                        "background-color: %1;")
                                        .arg(StyleManager::Colors::PRIMARY_ORANGE));

  setupViewControls();

  m_devToolsTabs = new QTabWidget(this);
  m_devToolsTabs->hide();

  setupConsole();
  setupDevTools();

  QWidget *fullViewContainer = new QWidget(this);
  QVBoxLayout *fullViewLayout = new QVBoxLayout(fullViewContainer);
  fullViewLayout->setContentsMargins(0, 0, 0, 0);
  fullViewLayout->setSpacing(0);

  m_splitter = new CustomSplitter(Qt::Horizontal, this);
  m_splitter->setHandleWidth(RESIZE_HANDLE_HEIGHT);
  m_splitter->setChildrenCollapsible(false);

  m_splitter->setStyleSheet("QSplitter { background: transparent; }");

  m_mainLayout->addSpacing(RESIZE_HANDLE_VISUAL_HEIGHT);
  m_mainLayout->addWidget(m_headerWidget);
  m_mainLayout->addWidget(fullViewContainer, 1);

  setStyleSheet(
      QString("DebugPane { "
              "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
              "    stop:0 %1, stop:0.1 %2, stop:0.2 %1, "
              "    stop:0.8 %1, stop:0.9 %2, stop:1 %1); "
              "  border-top: 2px solid %3; "
              "  border-bottom: 1px solid %4; "
              "}")
          .arg(StyleManager::Colors::blackAlpha(191))
          .arg(StyleManager::Colors::primaryOrangeAlpha(64))
          .arg(StyleManager::Colors::primaryOrangeAlpha(150))
          .arg(StyleManager::Colors::primaryOrangeAlpha(100)));

  m_slideAnimation = AnimationControl::createSlideAnimation(this, "slidePosition");
  connect(m_slideAnimation.get(), &QPropertyAnimation::finished,
          this, &DebugPane::animationFinished);

  updateViewMode();
}

void DebugPane::setupViewControls()
{
  m_headerWidget = new QWidget(this);
  m_headerWidget->setMouseTracking(true);
  m_headerWidget->setStyleSheet(StyleManager::consoleHeader());
  m_headerWidget->setMinimumHeight(30);

  m_animationBar = new QWidget(m_headerWidget);
  m_animationBar->hide();
  m_animationBar->setStyleSheet("background: transparent;");
  m_animationBar->setAttribute(Qt::WA_TransparentForMouseEvents);

  m_restartLabel = new QLabel("Tau5 Server Rebooting", m_headerWidget);
  m_restartLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  m_restartLabel->setStyleSheet("QLabel { color: white; font-family: 'Cascadia Code', 'Cascadia Mono', monospace; font-size: 13px; font-weight: normal; background: transparent; padding-left: 10px; }");
  m_restartLabel->setAttribute(Qt::WA_TranslucentBackground);
  m_restartLabel->hide();

  m_headerLayout = new QHBoxLayout(m_headerWidget);
  m_headerLayout->setContentsMargins(10, 2, 10, 2);

  m_restartButton = createCodiconButton(m_headerWidget, QChar(0xEB37), "Restart BEAM");
  m_resetButton = createCodiconButton(m_headerWidget, QChar(0xEB06), "Reset Dev Pane Browsers to Home");
  m_beamLogButton = createCodiconButton(m_headerWidget, QChar(0xEA85), "BEAM Log Only", true);
  m_devToolsButton = createCodiconButton(m_headerWidget, QChar(0xEAAF), "DevTools Only", true);
  m_sideBySideButton = createCodiconButton(m_headerWidget, QChar(0xEB56), "Side by Side View", true);
  m_closeButton = createCodiconButton(m_headerWidget, QChar(0xEA76), "Close Debug Pane");
  if (!m_devMode)
  {
    m_restartButton->setVisible(false);
    m_resetButton->setVisible(false);
  }

  m_restartButton->setFocusPolicy(Qt::NoFocus);
  m_resetButton->setFocusPolicy(Qt::NoFocus);
  m_beamLogButton->setFocusPolicy(Qt::NoFocus);
  m_devToolsButton->setFocusPolicy(Qt::NoFocus);
  m_sideBySideButton->setFocusPolicy(Qt::NoFocus);
  m_closeButton->setFocusPolicy(Qt::NoFocus);

  if (m_devMode)
  {
    m_headerLayout->addWidget(m_restartButton);
    m_headerLayout->addSpacing(8);
    m_headerLayout->addWidget(m_resetButton);
    m_headerLayout->addSpacing(20);
  }
  m_headerLayout->addStretch();

  m_headerLayout->addWidget(m_beamLogButton);
  m_headerLayout->addSpacing(4);
  m_headerLayout->addWidget(m_devToolsButton);
  m_headerLayout->addSpacing(4);
  m_headerLayout->addWidget(m_sideBySideButton);
  m_headerLayout->addSpacing(4);
  m_headerLayout->addWidget(m_closeButton);

  connect(m_restartButton, &QPushButton::clicked, this, &DebugPane::restartBeamRequested);
  connect(m_resetButton, &QPushButton::clicked, this, &DebugPane::resetDevPaneBrowsers);
  connect(m_beamLogButton, &QPushButton::clicked, this, &DebugPane::showBeamLogOnly);
  connect(m_devToolsButton, &QPushButton::clicked, this, &DebugPane::showDevToolsOnly);
  connect(m_sideBySideButton, &QPushButton::clicked, this, &DebugPane::showSideBySide);
  connect(m_closeButton, &QPushButton::clicked, this, &DebugPane::toggle);
}

void DebugPane::setupConsole()
{
  m_consoleContainer = new QWidget();
  QVBoxLayout *consoleMainLayout = new QVBoxLayout(m_consoleContainer);
  consoleMainLayout->setContentsMargins(0, 0, 0, 0);
  consoleMainLayout->setSpacing(0);

  QWidget *consoleHeaderContainer = new QWidget(m_consoleContainer);
  consoleHeaderContainer->setMaximumHeight(56);
  QVBoxLayout *headerLayout = new QVBoxLayout(consoleHeaderContainer);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(0);

  QWidget *consoleToolbar = ButtonUtilities::createTabToolbar(consoleHeaderContainer);
  QHBoxLayout *toolbarLayout = new QHBoxLayout(consoleToolbar);
  toolbarLayout->setContentsMargins(5, 2, 10, 2);
  toolbarLayout->setSpacing(2);

  m_bootLogTabButton = new ActivityTabButton("Boot", consoleToolbar);
  m_bootLogTabButton->setChecked(true);

  m_beamLogTabButton = new ActivityTabButton("BEAM Log", consoleToolbar);

  m_guiLogTabButton = new ActivityTabButton("GUI Log", consoleToolbar);

  bool enableDevMCP = isMcpEnabled();
  m_tau5MCPTabButton = new ActivityTabButton("Tau5 MCP", consoleToolbar);
  m_tidewaveMCPTabButton = new ActivityTabButton("Tidewave MCP", consoleToolbar);
  m_guiMCPTabButton = new ActivityTabButton("GUI MCP", consoleToolbar);

  if (!enableDevMCP)
  {
    m_guiMCPTabButton->setToolTip("GUI Dev MCP disabled - click for more information");
  }
  if (!m_devMode)
  {
    m_tidewaveMCPTabButton->setVisible(false);
    m_guiMCPTabButton->setVisible(false);
  }

  toolbarLayout->addWidget(m_bootLogTabButton);
  toolbarLayout->addWidget(m_beamLogTabButton);
  toolbarLayout->addWidget(m_guiLogTabButton);
  toolbarLayout->addWidget(m_tau5MCPTabButton);
  if (m_devMode)
  {
    toolbarLayout->addWidget(m_tidewaveMCPTabButton);
    toolbarLayout->addWidget(m_guiMCPTabButton);
  }
  toolbarLayout->addStretch();
  m_activityToggleButton = new QPushButton(consoleToolbar);
  m_activityToggleButton->setCheckable(true);
  m_activityToggleButton->setChecked(m_activityIndicatorsEnabled);
  m_activityToggleButton->setToolTip(m_activityIndicatorsEnabled
                                         ? "Activity indicators enabled (click to disable)"
                                         : "Activity indicators disabled (click to enable)");
  updateActivityToggleButtonStyle();
  m_activityToggleButton->setFixedSize(24, 24);
  m_activityToggleButton->setFocusPolicy(Qt::NoFocus);
  connect(m_activityToggleButton, &QPushButton::clicked, this, &DebugPane::toggleActivityIndicators);
  toolbarLayout->addWidget(m_activityToggleButton);

  headerLayout->addWidget(consoleToolbar);

  m_consoleToolbarStack = new QStackedWidget(consoleHeaderContainer);
  m_consoleToolbarStack->setMaximumHeight(28);
  headerLayout->addWidget(m_consoleToolbarStack);

  m_consoleStack = new QStackedWidget(m_consoleContainer);

  m_newBootLogWidget = new LogWidget(LogWidget::BootLog, nullptr);
  m_newBeamLogWidget = new LogWidget(LogWidget::BeamLog, nullptr);
  m_newGuiLogWidget = new LogWidget(LogWidget::GuiLog, nullptr);

  m_newTau5MCPWidget = new LogWidget(LogWidget::MCPLog, nullptr);
  m_newTidewaveMCPWidget = new LogWidget(LogWidget::MCPLog, nullptr);
  m_newGuiMCPWidget = new LogWidget(LogWidget::MCPLog, nullptr);

  QString tau5LogFilePath = Tau5Logger::instance().getMCPLogPath("tau5");
  m_newTau5MCPWidget->setLogFilePath(tau5LogFilePath);
  Tau5Logger::instance().debug(QString("DebugPane: Setting Tau5 MCP log path to: %1").arg(tau5LogFilePath));

  // Get the MCP port for Tau5 MCP
  QString tau5MCPPort = qgetenv("TAU5_MCP_PORT");
  if (tau5MCPPort.isEmpty()) {
    tau5MCPPort = "5555";
  }
  
  // Add startup message for Tau5 MCP
  QString tau5MCPStartupMessage =
      "\n"
      "═════════════════════════\n"
      "Tau5 MCP Server - ENABLED\n"
      "═════════════════════════\n"
      "\n"
      "Endpoint: http://localhost:" + tau5MCPPort + "/tau5/mcp\n"
      "\n"
      "To add to Claude Code, use:\n"
      "tau5 (http://localhost:" + tau5MCPPort + "/tau5/mcp)\n"
      "\n"
      "Available Tools:\n"
      "• Lua Evaluation (experimental)\n"
      "\n"
      "Note: All commands are executed in a secure\n"
      "sandboxed environment.\n"
      "\n─────────────────────────────────────────────\n";

  m_newTau5MCPWidget->appendLog(tau5MCPStartupMessage, false);

  if (enableDevMCP)
  {
    // Get the Chrome DevTools port that was set by applyEnvironmentVariables() from CLI args
    QString devToolsPort = qgetenv("TAU5_DEVTOOLS_PORT");
    if (devToolsPort.isEmpty()) {
      devToolsPort = "9223";  // Default port
    }
    
    QString guiMCPLogFilePath = Tau5Logger::getGlobalMCPLogPath(QString("gui-dev-%1").arg(devToolsPort));
    m_newGuiMCPWidget->setLogFilePath(guiMCPLogFilePath);
    Tau5Logger::instance().debug(QString("DebugPane: Setting GUI Dev MCP log path to: %1").arg(guiMCPLogFilePath));

    // Add startup message for GUI Dev MCP
    QString guiMCPStartupMessage =
        "\n"
        "═══════════════════════════════════\n"
        "Tau5-Dev GUI MCP Services - ENABLED\n"
        "═══════════════════════════════════\n"
        "\n"
        "Chrome DevTools Port: " + devToolsPort + "\n"
        "\n"
        "To add to Claude Code, use:\n"
        "tau5-gui-dev (stdio)\n"
        "\n";
    m_newGuiMCPWidget->appendLog(guiMCPStartupMessage, false);

    // Set up Tidewave MCP log
    QString tidewaveLogFilePath = Tau5Logger::instance().getMCPLogPath("tidewave");
    m_newTidewaveMCPWidget->setLogFilePath(tidewaveLogFilePath);
    Tau5Logger::instance().debug(QString("DebugPane: Setting Tidewave MCP log path to: %1").arg(tidewaveLogFilePath));

    // Get the MCP port from environment or use default
    QString mcpPort = qgetenv("TAU5_MCP_PORT");
    if (mcpPort.isEmpty()) {
      mcpPort = "5555";  // Default MCP port
    }
    
    QString tidewaveEnabledMessage =
        "\n"
        "══════════════════════════════════════\n"
        "Tau5-Dev Tidewave MCP Server - ENABLED\n"
        "══════════════════════════════════════\n"
        "\n"
        "Endpoint: http://localhost:" + mcpPort + "/tidewave/mcp\n"
        "\n"
        "To add to Claude Code, use:\n"
        "tidewave (http://localhost:" + mcpPort + "/tidewave/mcp)\n"
        "\n";
    m_newTidewaveMCPWidget->appendLog(tidewaveEnabledMessage, false);
  }
  else
  {
    QString tidewaveDisabledMessage =
        "\n"
        "═══════════════════════════════════════\n"
        "Tau5-Dev Tidewave MCP Server - DISABLED\n"
        "═══════════════════════════════════════\n"
        "\n"
        "\n"
        "To enable, start Tau5 with the flag\n"
        "  --enable-mcp\n";

    QString devGUIMCPDisabledMessage =
        "\n"
        "════════════════════════════════════\n"
        "Tau5-Dev GUI MCP Services - DISABLED\n"
        "════════════════════════════════════\n"
        "\n"
        "\n"
        "To enable, start Tau5 with the flag\n"
        "  --enable-mcp\n";

    m_newGuiMCPWidget->appendLog(devGUIMCPDisabledMessage, false);
    m_newTidewaveMCPWidget->appendLog(tidewaveDisabledMessage, false);
  }

  QString tidewaveMCPServerDescription =
      "\n"
      "The Tidewave MCP Server provides tools for:\n"
      "\n"
      "• Full Elixir control and observation\n"
      "• Code evaluation\n"
      "• Documentation lookup\n"
      "• BEAM Log access\n"
      "\n─────────────────────────────────────────────\n";

  // Get the Chrome DevTools port for the description
  QString devToolsPortDesc = qgetenv("TAU5_DEVTOOLS_PORT");
  if (devToolsPortDesc.isEmpty()) {
    devToolsPortDesc = "9223";
  }
  
  QString devGUIMCPServerDescription =
      "\n"
      "The Tau5-Dev GUI MCP Services provides access\n"
      "to the Chrome DevTools Protocol for:\n"
      "\n"
      "• DOM inspection and manipulation\n"
      "• JavaScript execution in WebViews\n"
      "• Element selection and style inspection\n"
      "\n"
      "Note: This service uses Chrome DevTools on port " + devToolsPortDesc + "\n"
      "and is accessed via the tau5-gui-dev MCP server.\n"
      "\n─────────────────────────────────────────────\n";

  m_newTidewaveMCPWidget->appendLog(tidewaveMCPServerDescription, false);
  m_newGuiMCPWidget->appendLog(devGUIMCPServerDescription, false);

  m_consoleStack->addWidget(m_newBootLogWidget);     // Index 0
  m_consoleStack->addWidget(m_newBeamLogWidget);     // Index 1
  m_consoleStack->addWidget(m_newGuiLogWidget);      // Index 2
  m_consoleStack->addWidget(m_newTau5MCPWidget);     // Index 3
  m_consoleStack->addWidget(m_newTidewaveMCPWidget); // Index 4
  m_consoleStack->addWidget(m_newGuiMCPWidget);      // Index 5

  m_consoleStack->setCurrentIndex(0);

  if (m_newBootLogWidget && m_newBootLogWidget->getToolbar())
  {
    m_consoleToolbarStack->addWidget(m_newBootLogWidget->getToolbar());
  }
  if (m_newBeamLogWidget && m_newBeamLogWidget->getToolbar())
  {
    m_consoleToolbarStack->addWidget(m_newBeamLogWidget->getToolbar());
  }
  if (m_newGuiLogWidget && m_newGuiLogWidget->getToolbar())
  {
    m_consoleToolbarStack->addWidget(m_newGuiLogWidget->getToolbar());
  }
  if (m_newTau5MCPWidget && m_newTau5MCPWidget->getToolbar())
  {
    m_consoleToolbarStack->addWidget(m_newTau5MCPWidget->getToolbar());
  }
  if (m_newTidewaveMCPWidget && m_newTidewaveMCPWidget->getToolbar())
  {
    m_consoleToolbarStack->addWidget(m_newTidewaveMCPWidget->getToolbar());
  }
  if (m_newGuiMCPWidget && m_newGuiMCPWidget->getToolbar())
  {
    m_consoleToolbarStack->addWidget(m_newGuiMCPWidget->getToolbar());
  }
  m_consoleToolbarStack->setCurrentIndex(0);

  consoleMainLayout->addWidget(consoleHeaderContainer);
  consoleMainLayout->addWidget(m_consoleStack);

  connect(m_bootLogTabButton, &QPushButton::clicked, this, &DebugPane::showBootLog);
  connect(m_beamLogTabButton, &QPushButton::clicked, this, &DebugPane::showBeamLog);
  connect(m_guiLogTabButton, &QPushButton::clicked, this, &DebugPane::showGuiLog);
  connect(m_tau5MCPTabButton, &QPushButton::clicked, this, &DebugPane::showTau5MCPLog);
  connect(m_tidewaveMCPTabButton, &QPushButton::clicked, this, &DebugPane::showTidewaveMCPLog);
  connect(m_guiMCPTabButton, &QPushButton::clicked, this, &DebugPane::showGuiMCPLog);

  connect(m_newBeamLogWidget, &LogWidget::logActivity,
          [this, widget = QPointer<LogWidget>(m_newBeamLogWidget),
           button = QPointer<ActivityTabButton>(m_beamLogTabButton)]()
          {
            if (widget && button)
              handleLogActivity(widget, button);
          });

  connect(m_newGuiLogWidget, &LogWidget::logActivity,
          [this, widget = QPointer<LogWidget>(m_newGuiLogWidget),
           button = QPointer<ActivityTabButton>(m_guiLogTabButton)]()
          {
            if (widget && button)
              handleLogActivity(widget, button);
          });

  connect(m_newTau5MCPWidget, &LogWidget::logActivity,
          [this, widget = QPointer<LogWidget>(m_newTau5MCPWidget),
           button = QPointer<ActivityTabButton>(m_tau5MCPTabButton)]()
          {
            if (widget && button)
              handleLogActivity(widget, button);
          });

  connect(m_newTidewaveMCPWidget, &LogWidget::logActivity,
          [this, widget = QPointer<LogWidget>(m_newTidewaveMCPWidget),
           button = QPointer<ActivityTabButton>(m_tidewaveMCPTabButton)]()
          {
            if (widget && button)
              handleLogActivity(widget, button);
          });

  connect(m_newGuiMCPWidget, &LogWidget::logActivity,
          [this, widget = QPointer<LogWidget>(m_newGuiMCPWidget),
           button = QPointer<ActivityTabButton>(m_guiMCPTabButton)]()
          {
            if (widget && button)
              handleLogActivity(widget, button);
          });
}

void DebugPane::setupDevTools()
{
  m_devToolsMainContainer = new QWidget(this);
  QVBoxLayout *devToolsMainLayout = new QVBoxLayout(m_devToolsMainContainer);
  devToolsMainLayout->setContentsMargins(0, 0, 0, 0);
  devToolsMainLayout->setSpacing(0);

  QWidget *devToolsToolbar = ButtonUtilities::createTabToolbar(m_devToolsMainContainer);

  QHBoxLayout *toolbarLayout = new QHBoxLayout(devToolsToolbar);
  toolbarLayout->setContentsMargins(5, 2, 10, 2);
  toolbarLayout->setSpacing(2);

  m_devToolsTabButton = ButtonUtilities::createTabButton("Dev Tools", devToolsToolbar);
  m_devToolsTabButton->setChecked(true);

  m_liveDashboardTabButton = ButtonUtilities::createTabButton("Live Dashboard", devToolsToolbar);

  m_elixirConsoleTabButton = ButtonUtilities::createTabButton("Elixir", devToolsToolbar);
  if (!m_devMode)
  {
    m_liveDashboardTabButton->setVisible(false);
    m_elixirConsoleTabButton->setVisible(false);
  }

  toolbarLayout->addWidget(m_devToolsTabButton);
  if (m_devMode)
  {
    toolbarLayout->addWidget(m_liveDashboardTabButton);
    toolbarLayout->addWidget(m_elixirConsoleTabButton);
  }
  toolbarLayout->addStretch();

  // Dev tools zoom buttons could be added here if needed
  // But since dev tools are web views, they have their own zoom controls

  m_devToolsStack = new QStackedWidget(m_devToolsMainContainer);

  m_devToolsContainer = new QWidget();
  m_devToolsContainer->setObjectName("debugwidget");
  QVBoxLayout *devToolsLayout = new QVBoxLayout(m_devToolsContainer);
  devToolsLayout->setContentsMargins(0, 0, 0, 0);
  devToolsLayout->setSpacing(0);

  m_devToolsView = new SandboxedWebView(m_devToolsContainer);
  m_devToolsView->setFallbackUrl(QUrl());
  m_devToolsView->page()->setBackgroundColor(QColor(StyleManager::Colors::DARK_BACKGROUND));

  QWebEngineSettings *devToolsSettings = m_devToolsView->settings();
  devToolsSettings->setFontFamily(QWebEngineSettings::FixedFont, "Cascadia Code");
  devToolsSettings->setFontSize(QWebEngineSettings::DefaultFixedFontSize, 14);

  DebugPaneThemeStyles::injectDevToolsFontScript(m_devToolsView);

  devToolsLayout->addWidget(m_devToolsView);

  m_liveDashboardContainer = new QWidget();
  m_liveDashboardContainer->setObjectName("debugwidget");
  QVBoxLayout *liveDashboardLayout = new QVBoxLayout(m_liveDashboardContainer);
  liveDashboardLayout->setContentsMargins(0, 0, 0, 0);
  liveDashboardLayout->setSpacing(0);

  m_liveDashboardView = new SandboxedWebView(m_liveDashboardContainer);
  m_liveDashboardView->page()->setBackgroundColor(QColor(StyleManager::Colors::DARK_BACKGROUND));
  liveDashboardLayout->addWidget(m_liveDashboardView);

  bool enableDevREPL = isElixirReplEnabled();
  QWidget *elixirWidget = nullptr;

  if (enableDevREPL)
  {
    m_elixirConsoleContainer = new QWidget();
    m_elixirConsoleContainer->setObjectName("debugwidget");
    QVBoxLayout *elixirConsoleLayout = new QVBoxLayout(m_elixirConsoleContainer);
    elixirConsoleLayout->setContentsMargins(0, 0, 0, 0);
    elixirConsoleLayout->setSpacing(0);

    m_elixirConsoleView = new PhxWebView(m_devMode, m_elixirConsoleContainer);
    m_elixirConsoleView->page()->setBackgroundColor(QColor(StyleManager::Colors::CONSOLE_BACKGROUND));
    elixirConsoleLayout->addWidget(m_elixirConsoleView);

    elixirWidget = m_elixirConsoleContainer;
  }
  else
  {
    LogWidget *elixirDisabledWidget = new LogWidget(LogWidget::GuiLog, nullptr);
    QString disabledMessage =
        "\nTau5 Elixir REPL Console - DISABLED\n"
        "═══════════════════════════════════\n\n"
        "The Tau5 Elixir REPL console is disabled for security.\n\n\n"
        "To enable the console, set TAU5_ELIXIR_REPL_ENABLED=true before starting Tau5.\n\n\n"
        "When enabled, you will have access to:\n\n"
        "• Interactive Elixir console\n"
        "• Direct server code execution\n"
        "• Runtime debugging capabilities\n"
        "• Access to all server modules and functions\n\n\n"
        "This feature should only be enabled in trusted development environments.\n";

    elixirDisabledWidget->appendLog(disabledMessage, false);
    elixirWidget = elixirDisabledWidget;

    m_elixirConsoleTabButton->setToolTip("Elixir REPL disabled - click for more information");
  }

  m_devToolsStack->addWidget(m_devToolsContainer);
  if (m_devMode)
  {
    m_devToolsStack->addWidget(m_liveDashboardContainer);
    m_devToolsStack->addWidget(elixirWidget);
  }
  m_devToolsStack->setCurrentIndex(0);

  devToolsMainLayout->addWidget(devToolsToolbar);
  devToolsMainLayout->addWidget(m_devToolsStack);

  connect(m_devToolsTabButton, &QPushButton::clicked, this, &DebugPane::showDevToolsTab);
  connect(m_liveDashboardTabButton, &QPushButton::clicked, this, &DebugPane::showLiveDashboardTab);
  connect(m_elixirConsoleTabButton, &QPushButton::clicked, this, &DebugPane::showElixirConsole);
}

void DebugPane::setWebView(PhxWebView *webView)
{
  m_targetWebView = webView;

  if (m_targetWebView && m_devToolsView)
  {
    QWebEnginePage *targetPage = m_targetWebView->page();
    if (targetPage)
    {
      targetPage->setDevToolsPage(m_devToolsView->page());

      DebugPaneThemeStyles::injectDevToolsFontScript(m_devToolsView);

      connect(m_devToolsView->page(), &QWebEnginePage::loadFinished, this, [this](bool ok)
              {
        if (ok) {
          DebugPaneThemeStyles::applyDevToolsDarkTheme(m_devToolsView);
          DebugPaneThemeStyles::injectDevToolsFontScript(m_devToolsView);

          emit webDevToolsLoaded();
        } });

      connect(m_targetWebView, &PhxWebView::inspectElementRequested,
              this, &DebugPane::handleInspectElementRequested);
    }
  }
}

void DebugPane::setViewMode(ViewMode mode)
{
  m_currentMode = mode;
  updateViewMode();
}

void DebugPane::updateViewMode()
{
  m_beamLogButton->setChecked(m_currentMode == BeamLogOnly);
  m_devToolsButton->setChecked(m_currentMode == DevToolsOnly);
  m_sideBySideButton->setChecked(m_currentMode == SideBySide);

  if (m_mainLayout->count() < 3)
    return;
  QWidget *fullViewContainer = qobject_cast<QWidget *>(m_mainLayout->itemAt(2)->widget());
  if (!fullViewContainer)
    return;

  QVBoxLayout *fullViewLayout = qobject_cast<QVBoxLayout *>(fullViewContainer->layout());
  if (!fullViewLayout)
    return;
  if (m_consoleContainer && m_consoleContainer->parent())
  {
    m_consoleContainer->setParent(nullptr);
  }
  if (m_devToolsMainContainer && m_devToolsMainContainer->parent())
  {
    m_devToolsMainContainer->setParent(nullptr);
  }

  while (fullViewLayout->count() > 0)
  {
    fullViewLayout->takeAt(0);
  }
  while (m_splitter->count() > 0)
  {
    m_splitter->widget(0)->setParent(nullptr);
  }

  switch (m_currentMode)
  {
  case BeamLogOnly:
    fullViewLayout->addWidget(m_consoleContainer);
    m_consoleContainer->show();
    m_splitter->hide();
    updateAllLogs();
    break;

  case DevToolsOnly:
    fullViewLayout->addWidget(m_devToolsMainContainer);
    m_devToolsMainContainer->show();
    m_splitter->hide();
    break;

  case SideBySide:
    m_splitter->addWidget(m_consoleContainer);
    m_consoleContainer->show();
    m_splitter->addWidget(m_devToolsMainContainer);
    m_splitter->setSizes({1000, 1000});
    fullViewLayout->addWidget(m_splitter);
    m_devToolsMainContainer->show();
    m_splitter->show();
    updateAllLogs();
    break;
  }
}

void DebugPane::showBeamLogOnly()
{
  setViewMode(BeamLogOnly);
}

void DebugPane::showDevToolsOnly()
{
  setViewMode(DevToolsOnly);
}

void DebugPane::showSideBySide()
{
  setViewMode(SideBySide);
}

void DebugPane::appendBootLog(const QString &text, bool isError)
{
  if (text.isEmpty())
    return;

  if (m_newBootLogWidget)
  {
    m_newBootLogWidget->appendLog(text, isError);
  }
}

void DebugPane::appendOutput(const QString &text, bool isError)
{
  if (text.isEmpty())
    return;

  if (m_newBeamLogWidget)
  {
    m_newBeamLogWidget->appendLog(text, isError);
  }
}

void DebugPane::toggle()
{
  slide(!m_isVisible);
}

void DebugPane::slide(bool show)
{
  if (show == m_isVisible || !parentWidget())
    return;

  int parentHeight = parentWidget()->height();
  int parentWidth = parentWidget()->width();
  int paneHeight = height();

  if (paneHeight <= 0)
  {
    paneHeight = parentHeight / 2;
  }

  paneHeight = constrainHeight(paneHeight);
  resize(parentWidth, paneHeight);

  if (show)
  {
    move(0, parentHeight);
    QWidget::show();
    raise();
  }

  AnimationControl::performSlide(m_slideAnimation.get(), show, parentHeight, paneHeight, height());
  m_isVisible = show;
}

void DebugPane::animationFinished()
{
  if (!m_isVisible)
  {
    hide();
  }
  else
  {
    raise();
  }
  emit visibilityChanged(m_isVisible);
}

bool DebugPane::eventFilter(QObject *obj, QEvent *event)
{
  Q_UNUSED(obj);
  Q_UNUSED(event);
  return QWidget::eventFilter(obj, event);
}

void DebugPane::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
  {
    QPoint pos = event->position().toPoint();
    bool inResizeArea = pos.y() < RESIZE_HANDLE_HEIGHT;
    bool inHeaderWidget = m_headerWidget && m_headerWidget->geometry().contains(pos);

    if (inResizeArea || inHeaderWidget)
    {
      m_isResizing = true;
      m_resizeStartY = event->globalPosition().y();
      m_resizeStartHeight = height();
      if (m_dragHandleWidget)
      {
        m_dragHandleWidget->show();
      }
      event->accept();
      return;
    }
  }

  QWidget::mousePressEvent(event);
}

void DebugPane::mouseMoveEvent(QMouseEvent *event)
{
  if (m_isResizing)
  {
    int deltaY = m_resizeStartY - event->globalPosition().y();
    int newHeight = m_resizeStartHeight + deltaY;
    newHeight = constrainHeight(newHeight);

    resize(width(), newHeight);
    move(x(), parentWidget()->height() - newHeight);
    event->accept();
  }
  else
  {
    bool wasHovering = m_isHoveringHandle;
    m_isHoveringHandle = (event->position().y() < RESIZE_HANDLE_HEIGHT);

    if (m_isHoveringHandle)
    {
      setCursor(Qt::SizeVerCursor);
      if (!wasHovering)
      {
        m_dragHandleWidget->show();
        m_dragHandleWidget->raise();
      }
    }
    else
    {
      setCursor(Qt::ArrowCursor);
      if (wasHovering)
      {
        m_dragHandleWidget->hide();
      }
    }
  }

  QWidget::mouseMoveEvent(event);
}

void DebugPane::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton && m_isResizing)
  {
    m_isResizing = false;
    if (!m_isHoveringHandle && m_dragHandleWidget)
    {
      m_dragHandleWidget->hide();
    }
    event->accept();
  }
  else
  {
    QWidget::mouseReleaseEvent(event);
  }
}

void DebugPane::enterEvent(QEnterEvent *event)
{
  QWidget::enterEvent(event);
}

void DebugPane::leaveEvent(QEvent *event)
{
  if (!m_isResizing)
  {
    setCursor(Qt::ArrowCursor);
    if (m_isHoveringHandle)
    {
      m_isHoveringHandle = false;
      m_dragHandleWidget->hide();
    }
  }
  QWidget::leaveEvent(event);
}

void DebugPane::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);

  if (parentWidget() && m_isVisible)
  {
    int constrainedHeight = constrainHeight(height());
    if (constrainedHeight != height())
    {
      resize(width(), constrainedHeight);
      move(x(), parentWidget()->height() - constrainedHeight);
    }
  }

  if (m_dragHandleWidget)
  {
    m_dragHandleWidget->resize(width(), RESIZE_HANDLE_VISUAL_HEIGHT);
    m_dragHandleWidget->move(0, 0);
  }

  if (m_animationBar && m_headerWidget)
  {
    m_animationBar->resize(m_headerWidget->size());
    m_animationBar->move(0, 0);
  }

  if (m_restartLabel && m_headerWidget)
  {
    m_restartLabel->resize(m_headerWidget->width(), m_headerWidget->height());
    m_restartLabel->move(0, 0);
  }
}

int DebugPane::constrainHeight(int requestedHeight) const
{
  if (!parentWidget())
    return requestedHeight;

  int minHeight = 100;
  int maxHeight = parentWidget()->height();

  return qBound(minHeight, requestedHeight, maxHeight);
}

void DebugPane::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);
}

void DebugPane::appendGuiLog(const QString &text, bool isError)
{
  if (text.isEmpty())
    return;

  if (m_newGuiLogWidget)
  {
    m_newGuiLogWidget->appendLog(text, isError);
  }
}

void DebugPane::setLiveDashboardUrl(const QString &url)
{
  if (!m_devMode)
  {
    return;
  }

  if (!url.isEmpty())
  {
    m_liveDashboardUrl = url;
  }

  if (m_liveDashboardView && !url.isEmpty())
  {
    QUrl dashboardUrl(url);
    m_liveDashboardView->setFallbackUrl(dashboardUrl);
    m_liveDashboardView->setUrl(dashboardUrl);

    connect(m_liveDashboardView->page(), &QWebEnginePage::loadFinished, this, [this](bool ok)
            {
      if (ok) {
        DebugPaneThemeStyles::applyLiveDashboardTau5Theme(m_liveDashboardView);
        emit liveDashboardLoaded();
      } });
  }
}

void DebugPane::setElixirConsoleUrl(const QString &url)
{
  if (!m_devMode)
  {
    return;
  }

  if (!url.isEmpty())
  {
    m_elixirConsoleUrl = url;
  }

  bool enableDevREPL = isElixirReplEnabled();

  if (enableDevREPL && m_elixirConsoleView && !url.isEmpty())
  {
    QUrl elixirConsoleUrl(url);
    Tau5Logger::instance().debug(QString("DebugPane::setElixirConsoleUrl - Setting URL: %1").arg(url));
    m_elixirConsoleView->setFallbackUrl(elixirConsoleUrl);
    m_elixirConsoleView->load(elixirConsoleUrl);

    connect(m_elixirConsoleView->page(), &QWebEnginePage::loadFinished, this, [this](bool ok)
            {
      if (ok) {
        DebugPaneThemeStyles::applyConsoleDarkTheme(m_elixirConsoleView);
        emit elixirConsoleLoaded();
      } });
  }
  else if (!enableDevREPL)
  {
    Tau5Logger::instance().info("DebugPane::setElixirConsoleUrl - Elixir REPL is disabled (TAU5_ENABLE_DEV_REPL not set)");
  }
  else
  {
    Tau5Logger::instance().warning("DebugPane::setElixirConsoleUrl - m_elixirConsoleView is null or url is empty");
  }
}

void DebugPane::showBootLog()
{
  if (m_consoleStack->currentWidget())
  {
    if (auto *debugWidget = qobject_cast<DebugWidget *>(m_consoleStack->currentWidget()))
    {
      debugWidget->onDeactivated();
    }
  }
  m_consoleStack->setCurrentIndex(0);
  if (m_consoleToolbarStack)
  {
    m_consoleToolbarStack->setCurrentIndex(0);
  }
  QList<QPushButton *> tabButtons = {m_bootLogTabButton, m_beamLogTabButton, m_guiLogTabButton, m_tau5MCPTabButton,
                                      m_tidewaveMCPTabButton, m_guiMCPTabButton};
  switchConsoleTab(0, tabButtons);
  if (m_newBootLogWidget)
  {
    m_newBootLogWidget->onActivated();
  }
}

void DebugPane::showBeamLog()
{
  if (m_consoleStack->currentWidget())
  {
    if (auto *debugWidget = qobject_cast<DebugWidget *>(m_consoleStack->currentWidget()))
    {
      debugWidget->onDeactivated();
    }
  }
  m_consoleStack->setCurrentIndex(1);
  if (m_consoleToolbarStack)
  {
    m_consoleToolbarStack->setCurrentIndex(1);
  }
  m_beamLogTabButton->setChecked(true);
  m_beamLogTabButton->setHasUnread(false);
  m_guiLogTabButton->setChecked(false);
  m_tau5MCPTabButton->setChecked(false);
  m_tidewaveMCPTabButton->setChecked(false);
  m_guiMCPTabButton->setChecked(false);
  if (m_newBeamLogWidget)
  {
    m_newBeamLogWidget->onActivated();
    m_newBeamLogWidget->setFocus();
  }
}

void DebugPane::showGuiLog()
{
  if (m_consoleStack->currentWidget())
  {
    if (auto *debugWidget = qobject_cast<DebugWidget *>(m_consoleStack->currentWidget()))
    {
      debugWidget->onDeactivated();
    }
  }
  // Switch to new LogWidget at index 2 (GUI log)
  m_consoleStack->setCurrentIndex(2);
  if (m_consoleToolbarStack)
  {
    m_consoleToolbarStack->setCurrentIndex(2);
  }
  m_bootLogTabButton->setChecked(false);
  m_beamLogTabButton->setChecked(false);
  m_guiLogTabButton->setChecked(true);
  m_guiLogTabButton->setHasUnread(false);
  m_tau5MCPTabButton->setChecked(false);
  m_tidewaveMCPTabButton->setChecked(false);
  m_guiMCPTabButton->setChecked(false);
  if (m_newGuiLogWidget)
  {
    m_newGuiLogWidget->onActivated();
    m_newGuiLogWidget->setFocus();
  }
}

void DebugPane::showElixirConsole()
{
  m_devToolsStack->setCurrentIndex(2);
  m_devToolsTabButton->setChecked(false);
  m_liveDashboardTabButton->setChecked(false);
  m_elixirConsoleTabButton->setChecked(true);
}

void DebugPane::showTau5MCPLog()
{
  if (m_consoleStack->currentWidget())
  {
    if (auto *debugWidget = qobject_cast<DebugWidget *>(m_consoleStack->currentWidget()))
    {
      debugWidget->onDeactivated();
    }
  }
  // Switch to new LogWidget at index 3 (Tau5 MCP)
  m_consoleStack->setCurrentIndex(3);
  if (m_consoleToolbarStack)
  {
    m_consoleToolbarStack->setCurrentIndex(3);
  }
  m_bootLogTabButton->setChecked(false);
  m_beamLogTabButton->setChecked(false);
  m_guiLogTabButton->setChecked(false);
  m_tau5MCPTabButton->setChecked(true);
  m_tau5MCPTabButton->setHasUnread(false);
  m_tidewaveMCPTabButton->setChecked(false);
  m_guiMCPTabButton->setChecked(false);
  if (m_newTau5MCPWidget)
  {
    m_newTau5MCPWidget->onActivated();
    m_newTau5MCPWidget->setFocus(); // Give focus for keyboard shortcuts
  }
}

void DebugPane::showTidewaveMCPLog()
{
  if (m_consoleStack->currentWidget())
  {
    if (auto *debugWidget = qobject_cast<DebugWidget *>(m_consoleStack->currentWidget()))
    {
      debugWidget->onDeactivated();
    }
  }
  // Switch to new LogWidget at index 4 (Tidewave MCP)
  m_consoleStack->setCurrentIndex(4);
  if (m_consoleToolbarStack)
  {
    m_consoleToolbarStack->setCurrentIndex(4);
  }
  m_bootLogTabButton->setChecked(false);
  m_beamLogTabButton->setChecked(false);
  m_guiLogTabButton->setChecked(false);
  m_tau5MCPTabButton->setChecked(false);
  m_tidewaveMCPTabButton->setChecked(true);
  m_tidewaveMCPTabButton->setHasUnread(false);
  m_guiMCPTabButton->setChecked(false);
  if (m_newTidewaveMCPWidget)
  {
    m_newTidewaveMCPWidget->onActivated();
    m_newTidewaveMCPWidget->setFocus();
  }
}

void DebugPane::showGuiMCPLog()
{
  if (m_consoleStack->currentWidget())
  {
    if (auto *debugWidget = qobject_cast<DebugWidget *>(m_consoleStack->currentWidget()))
    {
      debugWidget->onDeactivated();
    }
  }
  // Switch to new LogWidget at index 5 (GUI MCP)
  m_consoleStack->setCurrentIndex(5);
  if (m_consoleToolbarStack)
  {
    m_consoleToolbarStack->setCurrentIndex(5);
  }
  m_bootLogTabButton->setChecked(false);
  m_beamLogTabButton->setChecked(false);
  m_guiLogTabButton->setChecked(false);
  m_tau5MCPTabButton->setChecked(false);
  m_tidewaveMCPTabButton->setChecked(false);
  m_guiMCPTabButton->setChecked(true);
  m_guiMCPTabButton->setHasUnread(false);
  if (m_newGuiMCPWidget)
  {
    m_newGuiMCPWidget->onActivated();
    m_newGuiMCPWidget->setFocus();
  }
}

void DebugPane::switchConsoleTab(int index, const QList<QPushButton *> &tabButtons)
{
  for (int i = 0; i < tabButtons.size(); ++i)
  {
    tabButtons[i]->setChecked(i == index);
  }
  if (m_consoleStack)
  {
    m_consoleStack->setCurrentIndex(index);
  }
}

void DebugPane::showDevToolsTab()
{
  switchDevToolsTab(0);
}

void DebugPane::showLiveDashboardTab()
{
  switchDevToolsTab(1);
  if (m_liveDashboardView && m_liveDashboardView->page())
  {
    DebugPaneThemeStyles::applyLiveDashboardTau5Theme(m_liveDashboardView);
  }
}

void DebugPane::switchDevToolsTab(int index)
{
  if (m_devToolsTabButton)
    m_devToolsTabButton->setChecked(index == 0);
  if (m_liveDashboardTabButton)
    m_liveDashboardTabButton->setChecked(index == 1);
  if (m_elixirConsoleTabButton)
    m_elixirConsoleTabButton->setChecked(index == 2);
  if (m_devToolsStack)
    m_devToolsStack->setCurrentIndex(index);
}

void DebugPane::saveSettings()
{
  QSettings settings;
  settings.beginGroup("DebugPane");

  settings.setValue("visible", m_isVisible);
  settings.setValue("height", height());
  settings.setValue("viewMode", static_cast<int>(m_currentMode));

  if (m_currentMode == SideBySide && m_splitter)
  {
    settings.setValue("splitterSizes", m_splitter->saveState());
  }

  settings.setValue("beamLogFontSize", m_currentFontSize);
  settings.setValue("guiLogFontSize", m_guiLogFontSize);
  settings.setValue("consoleTabIndex", m_consoleStack->currentIndex());
  settings.setValue("devToolsTabIndex", m_devToolsStack->currentIndex());

  settings.endGroup();
  settings.sync();
}

void DebugPane::restoreSettings()
{
  QSettings settings;
  settings.beginGroup("DebugPane");

  if (settings.contains("height"))
  {
    int savedHeight = settings.value("height", parentWidget()->height() / 2).toInt();
    savedHeight = constrainHeight(savedHeight);
    resize(width(), savedHeight);
    if (parentWidget())
    {
      move(0, parentWidget()->height() - savedHeight);
    }
  }

  if (settings.contains("viewMode"))
  {
    ViewMode savedMode = static_cast<ViewMode>(settings.value("viewMode", BeamLogOnly).toInt());
    setViewMode(savedMode);
  }

  if (m_currentMode == SideBySide && m_splitter && settings.contains("splitterSizes"))
  {
    m_splitter->restoreState(settings.value("splitterSizes").toByteArray());
  }

  if (settings.contains("beamLogFontSize"))
  {
    m_currentFontSize = settings.value("beamLogFontSize", 12).toInt();
    if (m_newBeamLogWidget)
    {
      m_newBeamLogWidget->setFontSize(m_currentFontSize);
    }
  }

  if (settings.contains("guiLogFontSize"))
  {
    m_guiLogFontSize = settings.value("guiLogFontSize", 12).toInt();
    if (m_newGuiLogWidget)
    {
      m_newGuiLogWidget->setFontSize(m_guiLogFontSize);
    }
  }

  if (settings.contains("consoleTabIndex"))
  {
    int consoleIndex = settings.value("consoleTabIndex", 0).toInt();
    if (consoleIndex == 0)
    {
      showBeamLog();
    }
    else if (consoleIndex == 1)
    {
      showGuiLog();
    }
    else if (consoleIndex == 2)
    {
      showElixirConsole();
    }
  }

  if (settings.contains("devToolsTabIndex"))
  {
    int devToolsIndex = settings.value("devToolsTabIndex", 0).toInt();
    if (devToolsIndex == 0)
    {
      showDevToolsTab();
    }
    else
    {
      showLiveDashboardTab();
    }
  }

  settings.endGroup();
}

void DebugPane::setRestartButtonEnabled(bool enabled)
{
  if (!m_restartButton)
    return;

  m_restartButton->setEnabled(enabled);
  m_restartButton->setVisible(enabled);

  if (m_resetButton)
    m_resetButton->setVisible(enabled);
  if (m_beamLogButton)
    m_beamLogButton->setVisible(enabled);
  if (m_devToolsButton)
    m_devToolsButton->setVisible(enabled);
  if (m_sideBySideButton)
    m_sideBySideButton->setVisible(enabled);
  if (m_closeButton)
    m_closeButton->setVisible(enabled);

  if (m_beamLogTabButton)
    m_beamLogTabButton->setVisible(enabled);
  if (m_guiLogTabButton)
    m_guiLogTabButton->setVisible(enabled);
  if (m_tau5MCPTabButton)
    m_tau5MCPTabButton->setVisible(enabled);
  if (m_tidewaveMCPTabButton)
    m_tidewaveMCPTabButton->setVisible(enabled && m_devMode);
  if (m_guiMCPTabButton)
    m_guiMCPTabButton->setVisible(enabled && m_devMode);
  if (m_devToolsTabButton)
    m_devToolsTabButton->setVisible(enabled);
  if (m_liveDashboardTabButton)
    m_liveDashboardTabButton->setVisible(enabled);
  if (m_elixirConsoleTabButton)
    m_elixirConsoleTabButton->setVisible(enabled);

  if (enabled)
  {
    if (m_dragHandleAnimationTimer)
    {
      m_dragHandleAnimationTimer->stop();
      m_dragHandleAnimationTimer->deleteLater();
      m_dragHandleAnimationTimer = nullptr;

      if (m_animationBar)
      {
        m_animationBar->hide();
      }
    }

    if (m_restartLabel)
    {
      m_restartLabel->hide();
    }

    m_restartButton->setText(QChar(0xEB37));
    m_restartButton->setToolTip("Restart BEAM");
  }
  else
  {
    if (m_animationBar && !m_dragHandleAnimationTimer)
    {
      if (m_headerWidget)
      {
        m_animationBar->resize(m_headerWidget->size());
        m_animationBar->move(0, 0);
      }
      m_animationBar->show();

      class ChevronWidget : public QWidget
      {
      public:
        float scrollOffset = 0.0;
        float growthProgress = 0.0;

        ChevronWidget(QWidget *parent) : QWidget(parent)
        {
          setAttribute(Qt::WA_TranslucentBackground);
          setAttribute(Qt::WA_TransparentForMouseEvents);
        }

      protected:
        void paintEvent(QPaintEvent *) override
        {
          QPainter painter(this);
          painter.setRenderHint(QPainter::Antialiasing, false);

          int widgetWidth = width();
          int widgetHeight = height();

          int stripeWidth = 20;

          int leadingEdge = (widgetWidth + 100) * growthProgress;

          QPolygon clipShape;
          int triangleHeight = widgetHeight / 2;
          int triangleWidth = triangleHeight;

          if (leadingEdge <= triangleWidth)
          {
            float ratio = float(leadingEdge) / float(triangleWidth);
            clipShape << QPoint(0, widgetHeight / 2)
                      << QPoint(leadingEdge, widgetHeight / 2 - (triangleHeight * ratio))
                      << QPoint(leadingEdge, widgetHeight / 2 + (triangleHeight * ratio))
                      << QPoint(0, widgetHeight / 2);
          }
          else
          {
            int rectEnd = leadingEdge - triangleWidth;

            if (rectEnd > widgetWidth)
            {
              clipShape << QPoint(0, 0)
                        << QPoint(widgetWidth, 0)
                        << QPoint(widgetWidth, widgetHeight)
                        << QPoint(0, widgetHeight);
            }
            else
            {
              clipShape << QPoint(0, 0)
                        << QPoint(rectEnd, 0)
                        << QPoint(leadingEdge, widgetHeight / 2)
                        << QPoint(rectEnd, widgetHeight)
                        << QPoint(0, widgetHeight);
            }
          }
          painter.setClipRegion(clipShape);

          painter.fillRect(0, 0, widgetWidth, widgetHeight, QColor(25, 47, 217));

          int blockWidth = stripeWidth * 4;

          QColor deepPink(219, 39, 119);
          QColor yellow(255, 225, 25);

          int scrollPixels = -int(scrollOffset * blockWidth);

          painter.setRenderHint(QPainter::Antialiasing, false);

          const int chevronHeight = widgetHeight;
          const int chevronWidth = chevronHeight;

          for (int x = -chevronWidth * 2 - scrollPixels; x < widgetWidth + chevronWidth * 2; x += stripeWidth)
          {
            int stripeIndex = (x + scrollPixels + 10000) / stripeWidth;
            bool isPink = (stripeIndex % 2) == 0;

            QPolygon stripe;

            if ((stripeIndex / 2) % 2 == 0)
            {
              stripe << QPoint(x + chevronHeight / 2, chevronHeight / 2)
                     << QPoint(x + stripeWidth + chevronHeight / 2, chevronHeight / 2)
                     << QPoint(x + stripeWidth, 0)
                     << QPoint(x, 0);
            }
            else
            {
              stripe << QPoint(x + chevronHeight / 2, chevronHeight / 2)
                     << QPoint(x + stripeWidth + chevronHeight / 2, chevronHeight / 2)
                     << QPoint(x + stripeWidth, chevronHeight)
                     << QPoint(x, chevronHeight);
            }

            QPainterPath path;
            path.addPolygon(stripe);
            painter.fillPath(path, isPink ? deepPink : yellow);
          }
        }
      };

      if (m_animationBar)
      {
        m_animationBar->deleteLater();
      }

      ChevronWidget *chevronBar = new ChevronWidget(m_headerWidget);
      chevronBar->resize(m_headerWidget->size());
      chevronBar->move(0, 0);
      chevronBar->show();
      m_animationBar = chevronBar;

      m_dragHandleAnimationTimer = new QTimer(this);
      m_dragHandleAnimationTimer->setInterval(33);

      connect(m_dragHandleAnimationTimer, &QTimer::timeout, [this, chevronBar]()
              {
        if (!m_animationBar) return;

        chevronBar->scrollOffset += 0.005;
        if (chevronBar->scrollOffset > 1.0) chevronBar->scrollOffset -= 1.0;

        chevronBar->growthProgress = qMin(1.0f, chevronBar->growthProgress + 0.00333f);

        chevronBar->update(); });

      m_dragHandleAnimationTimer->start();
    }
  }
}

void DebugPane::keyPressEvent(QKeyEvent *event)
{
  QWidget::keyPressEvent(event);
}

void DebugPane::focusInEvent(QFocusEvent *event)
{
  QWidget::focusInEvent(event);
}

void DebugPane::focusOutEvent(QFocusEvent *event)
{
  QWidget::focusOutEvent(event);
}

void DebugPane::handleInspectElementRequested()
{
  if (!m_isVisible)
  {
    toggle();
  }

  if (m_currentMode == BeamLogOnly)
  {
    showDevToolsOnly();
  }
  else if (m_currentMode == SideBySide)
  {
    showDevToolsTab();
  }
  else
  {
    showDevToolsTab();
  }
}

void DebugPane::resetDevPaneBrowsers()
{
  Tau5Logger::instance().debug("DebugPane::resetDevPaneBrowsers - Resetting dev pane browsers");

  if (m_liveDashboardView && !m_liveDashboardUrl.isEmpty())
  {
    Tau5Logger::instance().debug(QString("Resetting Live Dashboard to: %1").arg(m_liveDashboardUrl));
    QUrl dashboardUrl(m_liveDashboardUrl);
    m_liveDashboardView->setUrl(dashboardUrl);
  }

  bool enableDevREPL = isElixirReplEnabled();
  if (enableDevREPL && m_elixirConsoleView && !m_elixirConsoleUrl.isEmpty())
  {
    Tau5Logger::instance().debug(QString("Resetting Elixir Console to: %1").arg(m_elixirConsoleUrl));
    QUrl consoleUrl(m_elixirConsoleUrl);
    m_elixirConsoleView->load(consoleUrl);
  }

  if (m_targetWebView && m_devToolsView)
  {
    Tau5Logger::instance().debug("Resetting DevTools connection");
    QWebEnginePage *targetPage = m_targetWebView->page();
    if (targetPage)
    {
      targetPage->setDevToolsPage(nullptr);
      targetPage->setDevToolsPage(m_devToolsView->page());

      connect(m_devToolsView->page(), &QWebEnginePage::loadFinished, this, [this](bool ok)
              {
        if (ok) {
          DebugPaneThemeStyles::applyDevToolsDarkTheme(m_devToolsView);
          DebugPaneThemeStyles::injectDevToolsFontScript(m_devToolsView);
        } }, Qt::SingleShotConnection);
    }
  }

  Tau5Logger::instance().info("Dev pane browsers have been reset");
}

void DebugPane::updateAllLogs()
{
  if (m_newBeamLogWidget)
  {
    m_newBeamLogWidget->updateIfNeeded();
  }

  if (m_newGuiLogWidget)
  {
    m_newGuiLogWidget->updateIfNeeded();
  }

  if (m_newTau5MCPWidget)
  {
    m_newTau5MCPWidget->updateIfNeeded();
  }

  if (m_newTidewaveMCPWidget)
  {
    m_newTidewaveMCPWidget->updateIfNeeded();
  }

  if (m_newGuiMCPWidget)
  {
    m_newGuiMCPWidget->updateIfNeeded();
  }
}

void DebugPane::toggleActivityIndicators()
{
  m_activityIndicatorsEnabled = !m_activityIndicatorsEnabled;

  if (m_beamLogTabButton)
  {
    m_beamLogTabButton->setActivityIndicatorsEnabled(m_activityIndicatorsEnabled);
    if (!m_activityIndicatorsEnabled)
    {
      m_beamLogTabButton->setHasUnread(false);
    }
  }

  if (m_guiLogTabButton)
  {
    m_guiLogTabButton->setActivityIndicatorsEnabled(m_activityIndicatorsEnabled);
    if (!m_activityIndicatorsEnabled)
    {
      m_guiLogTabButton->setHasUnread(false);
    }
  }

  if (m_tau5MCPTabButton)
  {
    m_tau5MCPTabButton->setActivityIndicatorsEnabled(m_activityIndicatorsEnabled);
    if (!m_activityIndicatorsEnabled)
    {
      m_tau5MCPTabButton->setHasUnread(false);
    }
  }

  if (m_tidewaveMCPTabButton)
  {
    m_tidewaveMCPTabButton->setActivityIndicatorsEnabled(m_activityIndicatorsEnabled);
    if (!m_activityIndicatorsEnabled)
    {
      m_tidewaveMCPTabButton->setHasUnread(false);
    }
  }

  if (m_guiMCPTabButton)
  {
    m_guiMCPTabButton->setActivityIndicatorsEnabled(m_activityIndicatorsEnabled);
    if (!m_activityIndicatorsEnabled)
    {
      m_guiMCPTabButton->setHasUnread(false);
    }
  }

  if (m_activityToggleButton)
  {
    m_activityToggleButton->setChecked(m_activityIndicatorsEnabled);
    m_activityToggleButton->setToolTip(m_activityIndicatorsEnabled
                                           ? "Activity indicators enabled (click to disable)"
                                           : "Activity indicators disabled (click to enable)");
    updateActivityToggleButtonStyle();
  }

  Tau5Logger::instance().info(QString("Activity indicators %1")
                                  .arg(m_activityIndicatorsEnabled ? "enabled" : "disabled"));
}

void DebugPane::handleLogActivity(LogWidget *widget, ActivityTabButton *button)
{
  if (!m_activityIndicatorsEnabled || !widget || !button)
  {
    return;
  }

  button->pulseActivity();
  if (m_consoleStack->currentWidget() != widget)
  {
    button->setHasUnread(true);
  }
}

void DebugPane::updateActivityToggleButtonStyle()
{
  if (!m_activityToggleButton)
  {
    return;
  }

  QString style = QString(
                      "QPushButton { "
                      "  background: transparent; "
                      "  border: none; "
                      "  color: %1; "
                      "  font-size: 14px; "
                      "  font-weight: normal; "
                      "  padding: 2px; "
                      "} "
                      "QPushButton:hover { "
                      "  background: %2; "
                      "  border-radius: 3px; "
                      "} "
                      "QPushButton:checked { "
                      "  color: %3; "
                      "  background: %4; "
                      "  border-radius: 3px; "
                      "}")
                      .arg(m_activityIndicatorsEnabled
                               ? StyleManager::Colors::PRIMARY_ORANGE
                               : StyleManager::Colors::TEXT_MUTED)
                      .arg(m_activityIndicatorsEnabled
                               ? StyleManager::Colors::primaryOrangeAlpha(25)
                               : "rgba(128, 128, 128, 0.1)")
                      .arg(StyleManager::Colors::PRIMARY_ORANGE)
                      .arg(m_activityIndicatorsEnabled
                               ? StyleManager::Colors::primaryOrangeAlpha(40)
                               : "rgba(128, 128, 128, 0.2)");

  m_activityToggleButton->setStyleSheet(style);
  m_activityToggleButton->setText(m_activityIndicatorsEnabled ? "⦿" : "⦾");
}

#include "moc_debugpane.cpp"
