// Icons used in this file are from Microsoft VS Code Icons
// https://github.com/microsoft/vscode-icons
// Licensed under CC BY 4.0: https://creativecommons.org/licenses/by/4.0/
#include "debugpane.h"
#include "debugpane/customsplitter.h"
#include "debugpane/searchfunctionality.h"
#include "debugpane/iconutilities.h"
#include "debugpane/buttonutilities.h"
#include "debugpane/themestyles.h"
#include "debugpane/zoomcontrol.h"
#include "debugpane/tabswitcher.h"
#include "debugpane/animationcontrol.h"
#include "../styles/StyleManager.h"
#include "phxwebview.h"
#include "sandboxedwebview.h"
#include "../logger.h"
#include "../lib/fontloader.h"
#include "../shortcuts/ShortcutManager.h"
#include <QShortcut>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
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
#include <cmath>

DebugPane::DebugPane(QWidget *parent)
    : QWidget(parent), m_isVisible(false), m_autoScroll(true), m_guiLogAutoScroll(true),
      m_maxLines(5000), m_currentMode(BeamLogOnly), m_isResizing(false),
      m_resizeStartY(0), m_resizeStartHeight(0), m_isHoveringHandle(false),
      m_targetWebView(nullptr), m_devToolsView(nullptr), m_liveDashboardView(nullptr),
      m_elixirConsoleView(nullptr), m_elixirConsoleTabButton(nullptr),
      m_currentFontSize(12), m_guiLogFontSize(12), m_devToolsMainContainer(nullptr),
      m_devToolsStack(nullptr), m_devToolsTabButton(nullptr), m_liveDashboardTabButton(nullptr),
      m_dragHandleWidget(nullptr), m_restartButton(nullptr), m_restartAnimationTimer(nullptr),
      m_beamLogSearchWidget(nullptr), m_beamLogSearchInput(nullptr),
      m_beamLogSearchCloseButton(nullptr), m_guiLogSearchWidget(nullptr), m_guiLogSearchInput(nullptr),
      m_guiLogSearchCloseButton(nullptr), m_beamLogSearchButton(nullptr), m_guiLogSearchButton(nullptr),
      m_searchFunctionality(new SearchFunctionality(this))
{
  setAttribute(Qt::WA_TranslucentBackground);
  setWindowFlags(Qt::FramelessWindowHint);
  setMouseTracking(true);
  setMinimumHeight(100);
  setupUi();
  setupShortcuts();
  hide();
}

DebugPane::~DebugPane() = default;

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
  m_headerWidget->setStyleSheet(StyleManager::consoleHeader());
  m_headerWidget->setMouseTracking(true);

  m_headerLayout = new QHBoxLayout(m_headerWidget);
  m_headerLayout->setContentsMargins(10, 2, 10, 2);

  QString normalColor = StyleManager::Colors::PRIMARY_ORANGE;
  QString hoverColor = StyleManager::Colors::WHITE;
  QString selectedColor = StyleManager::Colors::ERROR_BLUE;

  QIcon logIcon = IconUtilities::createSvgIcon(
      IconUtilities::Icons::terminalSvg(normalColor),
      "",
      IconUtilities::Icons::terminalSvg(selectedColor));

  QIcon devToolsIcon = IconUtilities::createSvgIcon(
      IconUtilities::Icons::bugSvg(normalColor),
      "",
      IconUtilities::Icons::bugSvg(selectedColor));

  QIcon sideBySideIcon = IconUtilities::createSvgIcon(
      IconUtilities::Icons::splitSvg(normalColor),
      "",
      IconUtilities::Icons::splitSvg(selectedColor));

  QIcon restartIcon = IconUtilities::createSvgIcon(
      IconUtilities::Icons::restartSvg(normalColor),
      IconUtilities::Icons::restartSvg(hoverColor),
      "");

  m_restartButton = new QPushButton(m_headerWidget);
  m_beamLogButton = new QPushButton(m_headerWidget);
  m_devToolsButton = new QPushButton(m_headerWidget);
  m_sideBySideButton = new QPushButton(m_headerWidget);

  m_restartButton->setIcon(restartIcon);
  m_beamLogButton->setIcon(logIcon);
  m_devToolsButton->setIcon(devToolsIcon);
  m_sideBySideButton->setIcon(sideBySideIcon);

  m_restartButton->setToolTip("Restart BEAM");
  m_beamLogButton->setToolTip("BEAM Log Only");
  m_devToolsButton->setToolTip("DevTools Only");
  m_sideBySideButton->setToolTip("Side by Side View");

  QString buttonStyle = ButtonUtilities::getHeaderButtonStyle();

  m_restartButton->setStyleSheet(buttonStyle);
  m_beamLogButton->setStyleSheet(buttonStyle);
  m_devToolsButton->setStyleSheet(buttonStyle);
  m_sideBySideButton->setStyleSheet(buttonStyle);

  m_beamLogButton->setCheckable(true);
  m_devToolsButton->setCheckable(true);
  m_sideBySideButton->setCheckable(true);

  m_restartButton->setFocusPolicy(Qt::NoFocus);
  m_beamLogButton->setFocusPolicy(Qt::NoFocus);
  m_devToolsButton->setFocusPolicy(Qt::NoFocus);
  m_sideBySideButton->setFocusPolicy(Qt::NoFocus);

  m_headerLayout->addWidget(m_restartButton);
  m_headerLayout->addSpacing(20);
  m_headerLayout->addStretch();

  m_headerLayout->addWidget(m_beamLogButton);
  m_headerLayout->addWidget(m_devToolsButton);
  m_headerLayout->addWidget(m_sideBySideButton);

  connect(m_restartButton, &QPushButton::clicked, this, &DebugPane::restartBeamRequested);
  connect(m_beamLogButton, &QPushButton::clicked, this, &DebugPane::showBeamLogOnly);
  connect(m_devToolsButton, &QPushButton::clicked, this, &DebugPane::showDevToolsOnly);
  connect(m_sideBySideButton, &QPushButton::clicked, this, &DebugPane::showSideBySide);
}

void DebugPane::setupConsole()
{
  m_consoleContainer = new QWidget();
  QVBoxLayout *consoleMainLayout = new QVBoxLayout(m_consoleContainer);
  consoleMainLayout->setContentsMargins(0, 0, 0, 0);
  consoleMainLayout->setSpacing(0);

  QWidget *consoleToolbar = ButtonUtilities::createTabToolbar(m_consoleContainer);

  QHBoxLayout *toolbarLayout = new QHBoxLayout(consoleToolbar);
  toolbarLayout->setContentsMargins(5, 2, 10, 2);
  toolbarLayout->setSpacing(2);

  m_beamLogTabButton = ButtonUtilities::createTabButton("BEAM Log", consoleToolbar);
  m_beamLogTabButton->setChecked(true);

  m_guiLogTabButton = ButtonUtilities::createTabButton("GUI Log", consoleToolbar);

  m_elixirConsoleTabButton = ButtonUtilities::createTabButton("Elixir", consoleToolbar);

  toolbarLayout->addWidget(m_beamLogTabButton);
  toolbarLayout->addWidget(m_guiLogTabButton);
  toolbarLayout->addWidget(m_elixirConsoleTabButton);
  toolbarLayout->addStretch();

  QString normalColor = StyleManager::Colors::PRIMARY_ORANGE;
  QString hoverColor = StyleManager::Colors::WHITE;
  QString activeColor = StyleManager::Colors::PRIMARY_ORANGE;
  QIcon autoScrollIcon;
  autoScrollIcon.addPixmap(IconUtilities::createSvgPixmap(IconUtilities::Icons::autoScrollOffSvg(normalColor), 20, 20), QIcon::Normal, QIcon::Off);
  autoScrollIcon.addPixmap(IconUtilities::createSvgPixmap(IconUtilities::Icons::autoScrollOffSvg(hoverColor), 20, 20), QIcon::Active, QIcon::Off);
  autoScrollIcon.addPixmap(IconUtilities::createSvgPixmap(IconUtilities::Icons::autoScrollOnSvg(normalColor), 20, 20), QIcon::Normal, QIcon::On);
  autoScrollIcon.addPixmap(IconUtilities::createSvgPixmap(IconUtilities::Icons::autoScrollOnSvg(hoverColor), 20, 20), QIcon::Active, QIcon::On);

  m_beamLogContainer = new QWidget();
  m_beamLogLayout = new QVBoxLayout(m_beamLogContainer);
  m_beamLogLayout->setContentsMargins(0, 0, 0, 0);
  m_beamLogLayout->setSpacing(0);

  m_autoScrollButton = new QPushButton(consoleToolbar);
  m_autoScrollButton->setIcon(autoScrollIcon);
  m_autoScrollButton->setCheckable(true);
  m_autoScrollButton->setChecked(true);
  m_autoScrollButton->setStyleSheet(QString(
                                        "QPushButton { "
                                        "  background: transparent; "
                                        "  border: none; "
                                        "  padding: 2px; "
                                        "  min-width: 16px; "
                                        "  max-width: 16px; "
                                        "  min-height: 16px; "
                                        "  max-height: 16px; "
                                        "} "
                                        "QPushButton:hover { "
                                        "  background: %1; "
                                        "}"
                                        "QPushButton:checked { "
                                        "  background: %2; "
                                        "  border-radius: 2px; "
                                        "}")
                                        .arg(StyleManager::Colors::primaryOrangeAlpha(25))
                                        .arg(StyleManager::Colors::primaryOrangeAlpha(64)));
  m_autoScrollButton->setToolTip("Auto-scroll");
  m_autoScrollButton->setFocusPolicy(Qt::NoFocus);
  m_autoScrollButton->setVisible(true);

  QIcon searchIcon;
  searchIcon.addPixmap(IconUtilities::createSvgPixmap(IconUtilities::Icons::searchSvg(normalColor), 13, 13), QIcon::Normal);
  searchIcon.addPixmap(IconUtilities::createSvgPixmap(IconUtilities::Icons::searchSvg(hoverColor), 13, 13), QIcon::Active);

  m_beamLogSearchButton = new QPushButton(consoleToolbar);
  m_beamLogSearchButton->setIcon(searchIcon);
  m_beamLogSearchButton->setCheckable(true);
  m_beamLogSearchButton->setStyleSheet(QString(
                                           "QPushButton { "
                                           "  background: transparent; "
                                           "  border: none; "
                                           "  padding: 2px; "
                                           "  min-width: 16px; "
                                           "  max-width: 16px; "
                                           "  min-height: 16px; "
                                           "  max-height: 16px; "
                                           "} "
                                           "QPushButton:hover { "
                                           "  background: %1; "
                                           "}"
                                           "QPushButton:checked { "
                                           "  background: %2; "
                                           "  border-radius: 2px; "
                                           "}")
                                           .arg(StyleManager::Colors::primaryOrangeAlpha(25))
                                           .arg(StyleManager::Colors::primaryOrangeAlpha(64)));
  m_beamLogSearchButton->setToolTip("Search (Ctrl+S)");
  m_beamLogSearchButton->setFocusPolicy(Qt::NoFocus);
  m_beamLogSearchButton->setVisible(true);

  QIcon zoomOutIcon;
  zoomOutIcon.addPixmap(IconUtilities::createSvgPixmap(IconUtilities::Icons::zoomOutSvg(normalColor), 16, 16), QIcon::Normal);
  zoomOutIcon.addPixmap(IconUtilities::createSvgPixmap(IconUtilities::Icons::zoomOutSvg(hoverColor), 16, 16), QIcon::Active);

  QIcon zoomInIcon;
  zoomInIcon.addPixmap(IconUtilities::createSvgPixmap(IconUtilities::Icons::zoomInSvg(normalColor), 16, 16), QIcon::Normal);
  zoomInIcon.addPixmap(IconUtilities::createSvgPixmap(IconUtilities::Icons::zoomInSvg(hoverColor), 16, 16), QIcon::Active);

  m_consoleZoomOutButton = ButtonUtilities::createZoomButton(zoomOutIcon, "Zoom Out", consoleToolbar);
  m_consoleZoomInButton = ButtonUtilities::createZoomButton(zoomInIcon, "Zoom In", consoleToolbar);
  m_consoleZoomOutButton->setVisible(true);
  m_consoleZoomInButton->setVisible(true);

  m_guiLogAutoScrollButton = new QPushButton(consoleToolbar);
  m_guiLogAutoScrollButton->setIcon(autoScrollIcon);
  m_guiLogAutoScrollButton->setCheckable(true);
  m_guiLogAutoScrollButton->setChecked(true);
  m_guiLogAutoScrollButton->setStyleSheet(QString(
                                              "QPushButton { "
                                              "  background: transparent; "
                                              "  border: none; "
                                              "  padding: 2px; "
                                              "  min-width: 16px; "
                                              "  max-width: 16px; "
                                              "  min-height: 16px; "
                                              "  max-height: 16px; "
                                              "} "
                                              "QPushButton:hover { "
                                              "  background: %1; "
                                              "}"
                                              "QPushButton:checked { "
                                              "  background: %2; "
                                              "  border-radius: 2px; "
                                              "}")
                                              .arg(StyleManager::Colors::primaryOrangeAlpha(25))
                                              .arg(StyleManager::Colors::primaryOrangeAlpha(64)));
  m_guiLogAutoScrollButton->setToolTip("Auto-scroll");
  m_guiLogAutoScrollButton->setFocusPolicy(Qt::NoFocus);
  m_guiLogAutoScrollButton->setVisible(false);

  m_guiLogSearchButton = new QPushButton(consoleToolbar);
  m_guiLogSearchButton->setIcon(searchIcon);
  m_guiLogSearchButton->setCheckable(true);
  m_guiLogSearchButton->setStyleSheet(QString(
                                          "QPushButton { "
                                          "  background: transparent; "
                                          "  border: none; "
                                          "  padding: 2px; "
                                          "  min-width: 16px; "
                                          "  max-width: 16px; "
                                          "  min-height: 16px; "
                                          "  max-height: 16px; "
                                          "} "
                                          "QPushButton:hover { "
                                          "  background: %1; "
                                          "}"
                                          "QPushButton:checked { "
                                          "  background: %2; "
                                          "  border-radius: 2px; "
                                          "}")
                                          .arg(StyleManager::Colors::primaryOrangeAlpha(25))
                                          .arg(StyleManager::Colors::primaryOrangeAlpha(64)));
  m_guiLogSearchButton->setToolTip("Search (Ctrl+S)");
  m_guiLogSearchButton->setFocusPolicy(Qt::NoFocus);
  m_guiLogSearchButton->setVisible(false);

  m_guiLogZoomOutButton = ButtonUtilities::createZoomButton(zoomOutIcon, "Zoom Out", consoleToolbar);
  m_guiLogZoomOutButton->setVisible(false);

  m_guiLogZoomInButton = ButtonUtilities::createZoomButton(zoomInIcon, "Zoom In", consoleToolbar);
  m_guiLogZoomInButton->setVisible(false);

  m_elixirConsoleZoomOutButton = ButtonUtilities::createZoomButton(zoomOutIcon, "Zoom Out", consoleToolbar);
  m_elixirConsoleZoomOutButton->setVisible(false);

  m_elixirConsoleZoomInButton = ButtonUtilities::createZoomButton(zoomInIcon, "Zoom In", consoleToolbar);
  m_elixirConsoleZoomInButton->setVisible(false);

  toolbarLayout->addWidget(m_beamLogSearchButton);
  toolbarLayout->addWidget(m_guiLogSearchButton);
  toolbarLayout->addWidget(m_autoScrollButton);
  toolbarLayout->addWidget(m_guiLogAutoScrollButton);
  toolbarLayout->addSpacing(5);
  toolbarLayout->addWidget(m_consoleZoomOutButton);
  toolbarLayout->addWidget(m_consoleZoomInButton);
  toolbarLayout->addWidget(m_guiLogZoomOutButton);
  toolbarLayout->addWidget(m_guiLogZoomInButton);
  toolbarLayout->addWidget(m_elixirConsoleZoomOutButton);
  toolbarLayout->addWidget(m_elixirConsoleZoomInButton);

  m_consoleStack = new QStackedWidget(m_consoleContainer);

  m_outputDisplay = new QTextEdit(m_beamLogContainer);
  m_outputDisplay->setReadOnly(true);
  m_outputDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_outputDisplay->setStyleSheet(StyleManager::consoleOutput());

  DebugPaneZoomControl::applyFontToTextEdit(m_outputDisplay, m_currentFontSize);

  m_beamLogLayout->addWidget(m_outputDisplay);

  m_beamLogSearchWidget = SearchFunctionality::createSearchWidget(m_beamLogContainer, m_beamLogSearchInput, m_beamLogSearchCloseButton);
  connect(m_beamLogSearchInput, &QLineEdit::textChanged, this, &DebugPane::performSearch);
  connect(m_beamLogSearchInput, &QLineEdit::returnPressed, this, &DebugPane::findNext);
  connect(m_beamLogSearchCloseButton, &QPushButton::clicked, this, &DebugPane::toggleSearchBar);

  m_guiLogContainer = new QWidget();
  m_guiLogLayout = new QVBoxLayout(m_guiLogContainer);
  m_guiLogLayout->setContentsMargins(0, 0, 0, 0);
  m_guiLogLayout->setSpacing(0);

  m_guiLogDisplay = new QTextEdit(m_guiLogContainer);
  m_guiLogDisplay->setReadOnly(true);
  m_guiLogDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_guiLogDisplay->setStyleSheet(StyleManager::consoleOutput());

  DebugPaneZoomControl::applyFontToTextEdit(m_guiLogDisplay, m_guiLogFontSize);

  m_guiLogLayout->addWidget(m_guiLogDisplay);

  m_guiLogSearchWidget = SearchFunctionality::createSearchWidget(m_guiLogContainer, m_guiLogSearchInput, m_guiLogSearchCloseButton);
  connect(m_guiLogSearchInput, &QLineEdit::textChanged, this, &DebugPane::performSearch);
  connect(m_guiLogSearchInput, &QLineEdit::returnPressed, this, &DebugPane::findNext);
  connect(m_guiLogSearchCloseButton, &QPushButton::clicked, this, &DebugPane::toggleSearchBar);

  m_elixirConsoleContainer = new QWidget();
  QVBoxLayout *elixirConsoleLayout = new QVBoxLayout(m_elixirConsoleContainer);
  elixirConsoleLayout->setContentsMargins(0, 0, 0, 0);
  elixirConsoleLayout->setSpacing(0);

  m_elixirConsoleView = new SandboxedWebView(m_elixirConsoleContainer);
  m_elixirConsoleView->page()->setBackgroundColor(QColor(StyleManager::Colors::CONSOLE_BACKGROUND));
  elixirConsoleLayout->addWidget(m_elixirConsoleView);

  m_consoleStack->addWidget(m_beamLogContainer);
  m_consoleStack->addWidget(m_guiLogContainer);
  m_consoleStack->addWidget(m_elixirConsoleContainer);
  m_consoleStack->setCurrentIndex(0);

  consoleMainLayout->addWidget(consoleToolbar);
  consoleMainLayout->addWidget(m_consoleStack);

  connect(m_beamLogTabButton, &QPushButton::clicked, this, &DebugPane::showBeamLog);
  connect(m_guiLogTabButton, &QPushButton::clicked, this, &DebugPane::showGuiLog);
  connect(m_elixirConsoleTabButton, &QPushButton::clicked, this, &DebugPane::showElixirConsole);

  connect(m_autoScrollButton, &QPushButton::toggled, this, &DebugPane::handleAutoScrollToggled);
  connect(m_beamLogSearchButton, &QPushButton::clicked, this, &DebugPane::toggleSearchBar);
  connect(m_consoleZoomInButton, &QPushButton::clicked, this, &DebugPane::handleConsoleZoomIn);
  connect(m_consoleZoomOutButton, &QPushButton::clicked, this, &DebugPane::handleConsoleZoomOut);

  connect(m_guiLogAutoScrollButton, &QPushButton::toggled, [this](bool checked)
          {
    m_guiLogAutoScroll = checked;
    if (checked) {
      QScrollBar *scrollBar = m_guiLogDisplay->verticalScrollBar();
      scrollBar->setValue(scrollBar->maximum());
    } });
  connect(m_guiLogSearchButton, &QPushButton::clicked, this, &DebugPane::toggleSearchBar);
  connect(m_guiLogZoomInButton, &QPushButton::clicked, this, &DebugPane::handleGuiLogZoomIn);
  connect(m_guiLogZoomOutButton, &QPushButton::clicked, this, &DebugPane::handleGuiLogZoomOut);

  connect(m_elixirConsoleZoomInButton, &QPushButton::clicked, this, &DebugPane::handleElixirConsoleZoomIn);
  connect(m_elixirConsoleZoomOutButton, &QPushButton::clicked, this, &DebugPane::handleElixirConsoleZoomOut);
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

  toolbarLayout->addWidget(m_devToolsTabButton);
  toolbarLayout->addWidget(m_liveDashboardTabButton);
  toolbarLayout->addStretch();

  QString devNormalColor = StyleManager::Colors::PRIMARY_ORANGE;
  QString devHoverColor = StyleManager::Colors::WHITE;
  QString devZoomOutSvg = QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'><path d='M3 8h10v1H3z'/></svg>");
  QIcon devZoomOutIcon;
  devZoomOutIcon.addPixmap(IconUtilities::createSvgPixmap(devZoomOutSvg.arg(devNormalColor), 16, 16), QIcon::Normal);
  devZoomOutIcon.addPixmap(IconUtilities::createSvgPixmap(devZoomOutSvg.arg(devHoverColor), 16, 16), QIcon::Active);

  QString devZoomInSvg = QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'><path d='M8 3v5H3v1h5v5h1V9h5V8H9V3H8z'/></svg>");
  QIcon devZoomInIcon;
  devZoomInIcon.addPixmap(IconUtilities::createSvgPixmap(devZoomInSvg.arg(devNormalColor), 16, 16), QIcon::Normal);
  devZoomInIcon.addPixmap(IconUtilities::createSvgPixmap(devZoomInSvg.arg(devHoverColor), 16, 16), QIcon::Active);

  m_zoomOutButton = ButtonUtilities::createZoomButton(devZoomOutIcon, "Zoom Out", devToolsToolbar);
  m_zoomOutButton->setVisible(true);

  m_zoomInButton = ButtonUtilities::createZoomButton(devZoomInIcon, "Zoom In", devToolsToolbar);
  m_zoomInButton->setVisible(true);

  m_liveDashboardZoomOutButton = ButtonUtilities::createZoomButton(devZoomOutIcon, "Zoom Out", devToolsToolbar);
  m_liveDashboardZoomOutButton->setVisible(false);

  m_liveDashboardZoomInButton = ButtonUtilities::createZoomButton(devZoomInIcon, "Zoom In", devToolsToolbar);
  m_liveDashboardZoomInButton->setVisible(false);

  toolbarLayout->addWidget(m_zoomOutButton);
  toolbarLayout->addWidget(m_zoomInButton);
  toolbarLayout->addWidget(m_liveDashboardZoomOutButton);
  toolbarLayout->addWidget(m_liveDashboardZoomInButton);

  m_devToolsStack = new QStackedWidget(m_devToolsMainContainer);

  m_devToolsContainer = new QWidget();
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
  QVBoxLayout *liveDashboardLayout = new QVBoxLayout(m_liveDashboardContainer);
  liveDashboardLayout->setContentsMargins(0, 0, 0, 0);
  liveDashboardLayout->setSpacing(0);

  m_liveDashboardView = new SandboxedWebView(m_liveDashboardContainer);
  m_liveDashboardView->page()->setBackgroundColor(QColor(StyleManager::Colors::DARK_BACKGROUND));
  liveDashboardLayout->addWidget(m_liveDashboardView);

  m_devToolsStack->addWidget(m_devToolsContainer);
  m_devToolsStack->addWidget(m_liveDashboardContainer);
  m_devToolsStack->setCurrentIndex(0);

  devToolsMainLayout->addWidget(devToolsToolbar);
  devToolsMainLayout->addWidget(m_devToolsStack);

  connect(m_devToolsTabButton, &QPushButton::clicked, this, &DebugPane::showDevToolsTab);
  connect(m_liveDashboardTabButton, &QPushButton::clicked, this, &DebugPane::showLiveDashboardTab);

  connect(m_zoomInButton, &QPushButton::clicked, this, &DebugPane::handleZoomIn);
  connect(m_zoomOutButton, &QPushButton::clicked, this, &DebugPane::handleZoomOut);

  connect(m_liveDashboardZoomInButton, &QPushButton::clicked, [this]()
          { DebugPaneZoomControl::zoomWebView(m_liveDashboardView, true); });

  connect(m_liveDashboardZoomOutButton, &QPushButton::clicked, [this]()
          { DebugPaneZoomControl::zoomWebView(m_liveDashboardView, false); });
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
    DebugPaneZoomControl::applyFontToTextEdit(m_outputDisplay, m_currentFontSize);
    DebugPaneZoomControl::applyFontToTextEdit(m_guiLogDisplay, m_guiLogFontSize);
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
    DebugPaneZoomControl::applyFontToTextEdit(m_outputDisplay, m_currentFontSize);
    DebugPaneZoomControl::applyFontToTextEdit(m_guiLogDisplay, m_guiLogFontSize);
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

void DebugPane::appendOutput(const QString &text, bool isError)
{
  if (text.isEmpty())
    return;

  QTextCursor cursor = m_outputDisplay->textCursor();
  cursor.movePosition(QTextCursor::End);

  QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss.zzz] ");

  QTextCharFormat format;
  format.setForeground(isError ? QColor(StyleManager::Colors::ERROR_BLUE) : QColor(StyleManager::Colors::PRIMARY_ORANGE));

  QTextCharFormat timestampFormat;
  timestampFormat.setForeground(QColor(StyleManager::Colors::TIMESTAMP_GRAY));
  cursor.setCharFormat(timestampFormat);
  cursor.insertText(timestamp);

  cursor.setCharFormat(format);
  cursor.insertText(text);

  if (!text.endsWith('\n'))
  {
    cursor.insertText("\n");
  }

  if (m_outputDisplay->document()->lineCount() > m_maxLines)
  {
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor,
                        m_outputDisplay->document()->lineCount() - m_maxLines);
    cursor.removeSelectedText();
  }

  if (m_autoScroll)
  {
    QScrollBar *scrollBar = m_outputDisplay->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
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

void DebugPane::handleAutoScrollToggled(bool checked)
{
  m_autoScroll = checked;
}

void DebugPane::handleZoomIn()
{
  DebugPaneZoomControl::zoomWebView(m_devToolsView, true);
}

void DebugPane::handleZoomOut()
{
  DebugPaneZoomControl::zoomWebView(m_devToolsView, false);
}

void DebugPane::handleConsoleZoomIn()
{
  DebugPaneZoomControl::zoomTextEdit(m_outputDisplay, m_currentFontSize, true);
}

void DebugPane::handleConsoleZoomOut()
{
  DebugPaneZoomControl::zoomTextEdit(m_outputDisplay, m_currentFontSize, false);
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
  if (event->button() == Qt::LeftButton &&
      m_headerWidget && m_headerWidget->geometry().contains(event->position().toPoint()))
  {
    m_isResizing = true;
    m_resizeStartY = event->globalPosition().y();
    m_resizeStartHeight = height();
    if (m_dragHandleWidget)
    {
      m_dragHandleWidget->show();
    }
    event->accept();
  }
  else
  {
    QWidget::mousePressEvent(event);
  }
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
    // Position the drag handle at the top of the widget
    m_dragHandleWidget->move(0, 0);
  }

  if (m_beamLogSearchWidget && m_beamLogSearchWidget->isVisible())
  {
    int x = m_beamLogContainer->width() - m_beamLogSearchWidget->width() - 20;
    int y = m_beamLogContainer->height() - m_beamLogSearchWidget->height() - 20;
    m_beamLogSearchWidget->move(x, y);
  }

  if (m_guiLogSearchWidget && m_guiLogSearchWidget->isVisible())
  {
    int x = m_guiLogContainer->width() - m_guiLogSearchWidget->width() - 20;
    int y = m_guiLogContainer->height() - m_guiLogSearchWidget->height() - 20;
    m_guiLogSearchWidget->move(x, y);
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

  QTextCursor cursor = m_guiLogDisplay->textCursor();
  cursor.movePosition(QTextCursor::End);

  QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss.zzz] ");

  QTextCharFormat format;
  format.setForeground(isError ? QColor(StyleManager::Colors::ERROR_BLUE) : QColor(StyleManager::Colors::PRIMARY_ORANGE));

  QTextCharFormat timestampFormat;
  timestampFormat.setForeground(QColor(StyleManager::Colors::TIMESTAMP_GRAY));
  cursor.setCharFormat(timestampFormat);
  cursor.insertText(timestamp);

  cursor.setCharFormat(format);
  cursor.insertText(text);

  if (!text.endsWith('\n'))
  {
    cursor.insertText("\n");
  }

  if (m_guiLogDisplay->document()->lineCount() > m_maxLines)
  {
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor,
                        m_guiLogDisplay->document()->lineCount() - m_maxLines);
    cursor.removeSelectedText();
  }

  if (m_guiLogAutoScroll)
  {
    QScrollBar *scrollBar = m_guiLogDisplay->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
  }
}

void DebugPane::setLiveDashboardUrl(const QString &url)
{
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
  if (m_elixirConsoleView && !url.isEmpty())
  {
    QUrl elixirConsoleUrl(url);
    Logger::log(Logger::Debug, QString("DebugPane::setElixirConsoleUrl - Setting URL: %1").arg(url));
    m_elixirConsoleView->setFallbackUrl(elixirConsoleUrl);
    m_elixirConsoleView->load(elixirConsoleUrl);

    connect(m_elixirConsoleView->page(), &QWebEnginePage::loadFinished, this, [this](bool ok)
            {
      if (ok) {
        DebugPaneThemeStyles::applyConsoleDarkTheme(m_elixirConsoleView);
        emit elixirConsoleLoaded();
      } });
  }
  else
  {
    Logger::log(Logger::Warning, "DebugPane::setElixirConsoleUrl - m_elixirConsoleView is null or url is empty");
  }
}

void DebugPane::handleGuiLogZoomIn()
{
  DebugPaneZoomControl::zoomTextEdit(m_guiLogDisplay, m_guiLogFontSize, true);
}

void DebugPane::handleGuiLogZoomOut()
{
  DebugPaneZoomControl::zoomTextEdit(m_guiLogDisplay, m_guiLogFontSize, false);
}

void DebugPane::handleElixirConsoleZoomIn()
{
  DebugPaneZoomControl::zoomWebView(m_elixirConsoleView, true);
}

void DebugPane::handleElixirConsoleZoomOut()
{
  DebugPaneZoomControl::zoomWebView(m_elixirConsoleView, false);
}

void DebugPane::showBeamLog()
{
  switchConsoleTab(0, {m_beamLogTabButton, m_guiLogTabButton, m_elixirConsoleTabButton});
}

void DebugPane::showGuiLog()
{
  switchConsoleTab(1, {m_beamLogTabButton, m_guiLogTabButton, m_elixirConsoleTabButton});
}

void DebugPane::showElixirConsole()
{
  switchConsoleTab(2, {m_beamLogTabButton, m_guiLogTabButton, m_elixirConsoleTabButton});
}

void DebugPane::switchConsoleTab(int index, const QList<QPushButton *> &tabButtons)
{
  QList<QPushButton *> allButtons = {
      m_autoScrollButton, m_beamLogSearchButton, m_consoleZoomOutButton, m_consoleZoomInButton,
      m_guiLogAutoScrollButton, m_guiLogSearchButton, m_guiLogZoomOutButton, m_guiLogZoomInButton,
      m_elixirConsoleZoomOutButton, m_elixirConsoleZoomInButton};

  TabSwitcher::switchConsoleTab(index, tabButtons, m_consoleStack, allButtons);
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
  QList<QPushButton *> zoomButtons = {
      m_zoomOutButton, m_zoomInButton,
      m_liveDashboardZoomOutButton, m_liveDashboardZoomInButton};

  TabSwitcher::switchDevToolsTab(index, m_devToolsTabButton, m_liveDashboardTabButton,
                                 m_devToolsStack, zoomButtons);
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
  settings.setValue("beamLogAutoScroll", m_autoScroll);
  settings.setValue("guiLogAutoScroll", m_guiLogAutoScroll);
  settings.setValue("consoleTabIndex", m_consoleStack->currentIndex());
  settings.setValue("devToolsTabIndex", m_devToolsStack->currentIndex());

  settings.endGroup();
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
    DebugPaneZoomControl::applyFontToTextEdit(m_outputDisplay, m_currentFontSize);
  }

  if (settings.contains("guiLogFontSize"))
  {
    m_guiLogFontSize = settings.value("guiLogFontSize", 12).toInt();
    DebugPaneZoomControl::applyFontToTextEdit(m_guiLogDisplay, m_guiLogFontSize);
  }

  m_autoScroll = settings.value("beamLogAutoScroll", true).toBool();
  m_autoScrollButton->setChecked(m_autoScroll);

  m_guiLogAutoScroll = settings.value("guiLogAutoScroll", true).toBool();
  m_guiLogAutoScrollButton->setChecked(m_guiLogAutoScroll);

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

  bool wasVisible = settings.value("visible", false).toBool();
  if (wasVisible)
  {
    m_isVisible = false;
  }

  settings.endGroup();
}

void DebugPane::setRestartButtonEnabled(bool enabled)
{
  if (!m_restartButton)
    return;

  m_restartButton->setEnabled(enabled);

  QString normalColor = StyleManager::Colors::PRIMARY_ORANGE;
  QString activeColor = StyleManager::Colors::WHITE;

  if (enabled)
  {
    if (m_restartAnimationTimer)
    {
      QString restartSvg = QString("<svg viewBox='0 0 16 16' fill='%1'><path d='M12.75 8a4.5 4.5 0 0 1-8.61 1.834l-1.391.565A6.001 6.001 0 0 0 14.25 8 6 6 0 0 0 3.5 4.334V2.5H2v4l.75.75h3.5v-1.5H4.352A4.5 4.5 0 0 1 12.75 8z'/></svg>");
      QIcon restartIcon = IconUtilities::createSvgIcon(
          restartSvg.arg(normalColor),
          restartSvg.arg(activeColor),
          "");
      AnimationControl::stopRestartAnimation(m_restartAnimationTimer, m_restartButton, restartIcon);
    }

    QString buttonStyle = QString(
                              "QPushButton { "
                              "  background: transparent; "
                              "  border: none; "
                              "  padding: 2px; "
                              "  margin: 0 2px; "
                              "  min-width: 24px; "
                              "  max-width: 24px; "
                              "  min-height: 16px; "
                              "  max-height: 16px; "
                              "} "
                              "QPushButton:hover { "
                              "  background: %1; "
                              "} "
                              "QPushButton:pressed { "
                              "  background: %2; "
                              "} "
                              "QPushButton:focus { "
                              "  outline: none; "
                              "}")
                              .arg(StyleManager::Colors::primaryOrangeAlpha(25))
                              .arg(StyleManager::Colors::primaryOrangeAlpha(51));
    m_restartButton->setStyleSheet(buttonStyle);

    QString restartSvg = QString("<svg viewBox='0 0 16 16' fill='%1'><path d='M12.75 8a4.5 4.5 0 0 1-8.61 1.834l-1.391.565A6.001 6.001 0 0 0 14.25 8 6 6 0 0 0 3.5 4.334V2.5H2v4l.75.75h3.5v-1.5H4.352A4.5 4.5 0 0 1 12.75 8z'/></svg>");
    QIcon restartIcon = IconUtilities::createSvgIcon(
        restartSvg.arg(normalColor),
        restartSvg.arg(activeColor),
        "");
    m_restartButton->setIcon(restartIcon);
    m_restartButton->setToolTip("Restart BEAM");
  }
  else
  {
    m_restartButton->setToolTip("BEAM restart in progress...");

    QString progressSvg = QString("<svg viewBox='0 0 16 16' fill='%1'>"
                                  "<path d='M8 1.5a6.5 6.5 0 1 0 6.5 6.5h1.5a8 8 0 1 1-8-8v1.5z'/>"
                                  "</svg>");
    QIcon progressIcon = IconUtilities::createSvgIcon(progressSvg.arg(normalColor), "", "");
    m_restartButton->setIcon(progressIcon);

    QString buttonStyle = QString(
        "QPushButton { "
        "  background: transparent; "
        "  border: none; "
        "  padding: 2px; "
        "  margin: 0 2px; "
        "  min-width: 24px; "
        "  max-width: 24px; "
        "  min-height: 16px; "
        "  max-height: 16px; "
        "} "
        "QPushButton:disabled { "
        "  background: transparent; "
        "}");
    m_restartButton->setStyleSheet(buttonStyle);

    if (!m_restartAnimationTimer)
    {
      QString progressSvg = QString("<svg viewBox='0 0 16 16' fill='%1'>"
                                    "<path d='M8 1.5a6.5 6.5 0 1 0 6.5 6.5h1.5a8 8 0 1 1-8-8v1.5z'/>"
                                    "</svg>");
      QIcon progressIcon = IconUtilities::createSvgIcon(progressSvg.arg(normalColor), "", "");

      m_restartAnimationTimer = AnimationControl::createRestartAnimation(this, m_restartButton, progressIcon);
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

void DebugPane::setupShortcuts()
{
  ShortcutManager &mgr = ShortcutManager::instance();

  // Search/Find Next shortcut (Ctrl+S)
  QShortcut *searchShortcut = new QShortcut(mgr.getKeySequence(ShortcutManager::DebugPaneSearch), this);
  searchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(searchShortcut, &QShortcut::activated, this, &DebugPane::handleSearchShortcut);
  m_shortcuts.append(searchShortcut);

  // Find Previous shortcut (Ctrl+R)
  QShortcut *findPrevShortcut = new QShortcut(mgr.getKeySequence(ShortcutManager::DebugPaneFindPrevious), this);
  findPrevShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(findPrevShortcut, &QShortcut::activated, this, &DebugPane::findPrevious);
  m_shortcuts.append(findPrevShortcut);

  // Close Search shortcut (Ctrl+G)
  QShortcut *closeSearchShortcut = new QShortcut(mgr.getKeySequence(ShortcutManager::DebugPaneCloseSearch), this);
  closeSearchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(closeSearchShortcut, &QShortcut::activated, this, &DebugPane::toggleSearchBar);
  m_shortcuts.append(closeSearchShortcut);
}

void DebugPane::cleanupShortcuts()
{
  // Shortcuts are automatically cleaned up when the widget is destroyed
}

void DebugPane::getCurrentSearchContext(QWidget *&searchWidget, QLineEdit *&searchInput, QTextEdit *&textEdit, QString *&lastSearchText)
{
  searchWidget = nullptr;
  searchInput = nullptr;
  textEdit = nullptr;
  lastSearchText = nullptr;

  if (m_consoleStack->currentIndex() == 0)
  {
    searchWidget = m_beamLogSearchWidget;
    searchInput = m_beamLogSearchInput;
    textEdit = m_outputDisplay;
    lastSearchText = &m_beamLogLastSearchText;
  }
  else if (m_consoleStack->currentIndex() == 1)
  {
    searchWidget = m_guiLogSearchWidget;
    searchInput = m_guiLogSearchInput;
    textEdit = m_guiLogDisplay;
    lastSearchText = &m_guiLogLastSearchText;
  }
}

void DebugPane::highlightAllMatches(QTextEdit *textEdit, const QString &searchText, const QTextCursor &currentMatch)
{
  SearchFunctionality::highlightAllMatches(textEdit, searchText, currentMatch);
}

void DebugPane::toggleSearchBar()
{
  QWidget *searchWidget = nullptr;
  QLineEdit *searchInput = nullptr;
  QTextEdit *textEdit = nullptr;
  QString *lastSearchText = nullptr;

  getCurrentSearchContext(searchWidget, searchInput, textEdit, lastSearchText);

  if (!searchWidget || !searchInput || !textEdit)
    return;

  int currentTab = m_consoleStack->currentIndex();
  QPushButton *searchButton = (currentTab == 0) ? m_beamLogSearchButton : m_guiLogSearchButton;
  QWidget *container = (currentTab == 0) ? m_beamLogContainer : m_guiLogContainer;

  SearchFunctionality::SearchContext context = {
      searchWidget,
      searchInput,
      textEdit,
      lastSearchText,
      searchButton};

  m_searchFunctionality->toggleSearchBar(context, container);
}

void DebugPane::performSearch()
{
  QLineEdit *searchInput = qobject_cast<QLineEdit *>(sender());
  if (!searchInput)
    return;

  QTextEdit *currentTextEdit = nullptr;
  QString *lastSearchText = nullptr;

  if (searchInput == m_beamLogSearchInput)
  {
    currentTextEdit = m_outputDisplay;
    lastSearchText = &m_beamLogLastSearchText;
  }
  else if (searchInput == m_guiLogSearchInput)
  {
    currentTextEdit = m_guiLogDisplay;
    lastSearchText = &m_guiLogLastSearchText;
  }

  if (!currentTextEdit || !lastSearchText)
    return;

  m_searchFunctionality->performSearch(searchInput, currentTextEdit, *lastSearchText);
}

void DebugPane::handleSearchShortcut()
{
  if (!m_isVisible)
    return;

  QWidget *searchWidget = (m_consoleStack->currentIndex() == 0) ? m_beamLogSearchWidget : m_guiLogSearchWidget;

  if (searchWidget && searchWidget->isVisible())
  {
    findNext();
  }
  else
  {
    toggleSearchBar();
  }
}

void DebugPane::findNext()
{
  QWidget *searchWidget = nullptr;
  QLineEdit *searchInput = nullptr;
  QTextEdit *currentTextEdit = nullptr;
  QString *lastSearchText = nullptr;

  getCurrentSearchContext(searchWidget, searchInput, currentTextEdit, lastSearchText);

  if (!searchWidget || !searchInput || !currentTextEdit || !lastSearchText)
    return;

  SearchFunctionality::SearchContext context = {
      searchWidget,
      searchInput,
      currentTextEdit,
      lastSearchText,
      nullptr};

  m_searchFunctionality->findNext(context);
}

void DebugPane::findPrevious()
{
  QWidget *searchWidget = nullptr;
  QLineEdit *searchInput = nullptr;
  QTextEdit *currentTextEdit = nullptr;
  QString *lastSearchText = nullptr;

  getCurrentSearchContext(searchWidget, searchInput, currentTextEdit, lastSearchText);

  if (!searchWidget || !searchInput || !currentTextEdit || !lastSearchText)
    return;

  SearchFunctionality::SearchContext context = {
      searchWidget,
      searchInput,
      currentTextEdit,
      lastSearchText,
      nullptr};

  m_searchFunctionality->findPrevious(context);
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

#include "moc_debugpane.cpp"
