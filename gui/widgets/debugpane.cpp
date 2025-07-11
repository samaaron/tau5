// Icons used in this file are inspired by Microsoft VS Code Icons
// https://github.com/microsoft/vscode-icons
// Licensed under CC BY 4.0: https://creativecommons.org/licenses/by/4.0/
#include "debugpane.h"
#include "StyleManager.h"
#include "phxwebview.h"
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
#include <QRegularExpression>
#include <QSettings>
#include <QFile>
#include <QTextStream>

DebugPane::DebugPane(QWidget *parent)
    : QWidget(parent), m_isVisible(false), m_autoScroll(true), m_guiLogAutoScroll(true),
      m_maxLines(5000), m_currentMode(BeamLogOnly), m_isResizing(false), 
      m_resizeStartY(0), m_resizeStartHeight(0), m_targetWebView(nullptr), 
      m_devToolsView(nullptr), m_liveDashboardView(nullptr),
      m_currentFontSize(12), m_guiLogFontSize(12), m_devToolsMainContainer(nullptr), 
      m_devToolsStack(nullptr), m_devToolsTabButton(nullptr), m_liveDashboardTabButton(nullptr)
{
  setAttribute(Qt::WA_TranslucentBackground);
  setWindowFlags(Qt::FramelessWindowHint);
  setMouseTracking(true);
  setMinimumHeight(100);
  setupUi();
  hide();
}

DebugPane::~DebugPane() = default;

void DebugPane::setupUi()
{
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  m_mainLayout->setSpacing(0);

  setupViewControls();
  
  m_devToolsTabs = new QTabWidget(this);
  m_devToolsTabs->hide();
  
  setupConsole();
  setupDevTools();

  QWidget *fullViewContainer = new QWidget(this);
  QVBoxLayout *fullViewLayout = new QVBoxLayout(fullViewContainer);
  fullViewLayout->setContentsMargins(0, 0, 0, 0);
  fullViewLayout->setSpacing(0);
  
  m_splitter = new QSplitter(Qt::Horizontal, this);

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

  m_slideAnimation = std::make_unique<QPropertyAnimation>(this, "slidePosition");
  m_slideAnimation->setDuration(300);
  m_slideAnimation->setEasingCurve(QEasingCurve::OutCubic);
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
  m_headerLayout->setContentsMargins(10, 5, 10, 5);


  QString normalColor = StyleManager::Colors::PRIMARY_ORANGE;
  QString hoverColor = StyleManager::Colors::WHITE;
  QString selectedColor = StyleManager::Colors::ERROR_BLUE;

  QString terminalSvg = QString("<svg viewBox='0 0 24 24' fill='%1'><path fill-rule='evenodd' clip-rule='evenodd' d='M1.5 3L3 1.5H21L22.5 3V21L21 22.5H3L1.5 21V3ZM3 3V21H21V3H3Z'/><path d='M7.06078 7.49988L6.00012 8.56054L10.2427 12.8032L6 17.0459L7.06066 18.1066L12 13.1673V12.4391L7.06078 7.49988Z'/><rect x='12' y='16.5' width='6' height='1.5'/></svg>");

  QString listSvg = QString("<svg viewBox='0 0 16 16' fill='%1'><rect x='4' y='9' width='9' height='1'/><rect x='4' y='12' width='7' height='1'/><rect x='4' y='6' width='10' height='1'/><rect x='1' y='3' width='11' height='1'/><rect x='4' y='4' width='1' height='9'/></svg>");

  QIcon logIcon = createSvgIcon(
      terminalSvg.arg(normalColor),
      "",
      terminalSvg.arg(selectedColor));

  QString bugSvg = QString("<svg viewBox='0 0 16 16' fill='%1'><path fill-rule='evenodd' clip-rule='evenodd' d='M10.877 4.5v-.582a2.918 2.918 0 1 0-5.836 0V4.5h-.833L2.545 2.829l-.593.59 1.611 1.619-.019.049a8.03 8.03 0 0 0-.503 2.831c0 .196.007.39.02.58l.003.045H1v.836h2.169l.006.034c.172.941.504 1.802.954 2.531l.034.055L2.2 13.962l.592.592 1.871-1.872.058.066c.868.992 2.002 1.589 3.238 1.589 1.218 0 2.336-.579 3.199-1.544l.057-.064 1.91 1.92.593-.591-1.996-2.006.035-.056c.467-.74.81-1.619.986-2.583l.006-.034h2.171v-.836h-2.065l.003-.044a8.43 8.43 0 0 0 .02-.58 8.02 8.02 0 0 0-.517-2.866l-.019-.05 1.57-1.57-.592-.59L11.662 4.5h-.785zm-5 0v-.582a2.082 2.082 0 1 1 4.164 0V4.5H5.878z'/></svg>");

  QIcon devToolsIcon = createSvgIcon(
      bugSvg.arg(normalColor),
      "",
      bugSvg.arg(selectedColor));

  QString splitSvg = QString("<svg viewBox='0 0 16 16' fill='%1'><path d='M14 1H3L2 2v11l1 1h11l1-1V2l-1-1zM8 13H3V2h5v11zm6 0H9V2h5v11z'/></svg>");
  QIcon sideBySideIcon = createSvgIcon(
      splitSvg.arg(normalColor),
      "",
      splitSvg.arg(selectedColor));

  m_beamLogButton = new QPushButton(m_headerWidget);
  m_devToolsButton = new QPushButton(m_headerWidget);
  m_sideBySideButton = new QPushButton(m_headerWidget);

  m_beamLogButton->setIcon(logIcon);
  m_devToolsButton->setIcon(devToolsIcon);
  m_sideBySideButton->setIcon(sideBySideIcon);

  m_beamLogButton->setToolTip("BEAM Log Only");
  m_devToolsButton->setToolTip("DevTools Only");
  m_sideBySideButton->setToolTip("Side by Side View");

  QString buttonStyle = QString(
      "QPushButton { "
      "  background: transparent; "
      "  border: none; "
      "  padding: 4px; "
      "  margin: 0 2px; "
      "  min-width: 32px; "
      "  max-width: 32px; "
      "  min-height: 32px; "
      "  max-height: 32px; "
      "} "
      "QPushButton:hover { "
      "  background: rgba(255, 165, 0, 0.1); "
      "} "
      "QPushButton:pressed { "
      "  background: rgba(255, 165, 0, 0.2); "
      "} "
      "QPushButton:checked { "
      "  background: rgba(65, 105, 225, 0.2); "
      "  border-radius: 3px; "
      "} "
      "QPushButton:focus { "
      "  outline: none; "
      "}");

  m_beamLogButton->setStyleSheet(buttonStyle);
  m_devToolsButton->setStyleSheet(buttonStyle);
  m_sideBySideButton->setStyleSheet(buttonStyle);

  m_beamLogButton->setCheckable(true);
  m_devToolsButton->setCheckable(true);
  m_sideBySideButton->setCheckable(true);
  
  m_beamLogButton->setFocusPolicy(Qt::NoFocus);
  m_devToolsButton->setFocusPolicy(Qt::NoFocus);
  m_sideBySideButton->setFocusPolicy(Qt::NoFocus);

  m_headerLayout->addStretch();

  m_headerLayout->addWidget(m_beamLogButton);
  m_headerLayout->addWidget(m_devToolsButton);
  m_headerLayout->addWidget(m_sideBySideButton);

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
  
  QWidget *consoleToolbar = createTabToolbar(m_consoleContainer);
  
  QHBoxLayout *toolbarLayout = new QHBoxLayout(consoleToolbar);
  toolbarLayout->setContentsMargins(5, 2, 10, 2);
  toolbarLayout->setSpacing(2);
  
  m_beamLogTabButton = createTabButton("BEAM Log", consoleToolbar);
  m_beamLogTabButton->setChecked(true);
  
  m_guiLogTabButton = createTabButton("GUI Log", consoleToolbar);
  
  toolbarLayout->addWidget(m_beamLogTabButton);
  toolbarLayout->addWidget(m_guiLogTabButton);
  toolbarLayout->addStretch();
  
  QString normalColor = StyleManager::Colors::PRIMARY_ORANGE;
  QString hoverColor = StyleManager::Colors::WHITE;
  QString activeColor = StyleManager::Colors::PRIMARY_ORANGE;
  QString autoScrollOffSvg = QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'>"
      "<path d='M8 3v7M5 7l3 3 3-3' stroke='%1' stroke-width='1.5' fill='none'/>"
      "<rect x='4' y='12' width='8' height='2' fill='%1' opacity='0.3'/>"
      "</svg>");
  
  QString autoScrollOnSvg = QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'>"
      "<path d='M8 3v7M5 7l3 3 3-3' stroke='%1' stroke-width='1.5' fill='none'/>"
      "<rect x='4' y='12' width='8' height='2' fill='%1'/>"
      "</svg>");
  
  QIcon autoScrollIcon;
  autoScrollIcon.addPixmap(createSvgPixmap(autoScrollOffSvg.arg(normalColor), 20, 20), QIcon::Normal, QIcon::Off);
  autoScrollIcon.addPixmap(createSvgPixmap(autoScrollOffSvg.arg(hoverColor), 20, 20), QIcon::Active, QIcon::Off);
  autoScrollIcon.addPixmap(createSvgPixmap(autoScrollOnSvg.arg(normalColor), 20, 20), QIcon::Normal, QIcon::On);
  autoScrollIcon.addPixmap(createSvgPixmap(autoScrollOnSvg.arg(hoverColor), 20, 20), QIcon::Active, QIcon::On);
  
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
      "  background: rgba(255, 165, 0, 0.1); "
      "}"
      "QPushButton:checked { "
      "  background: rgba(255, 165, 0, 0.25); "
      "  border-radius: 2px; "
      "}"));
  m_autoScrollButton->setToolTip("Auto-scroll");
  m_autoScrollButton->setFocusPolicy(Qt::NoFocus);
  m_autoScrollButton->setVisible(true);
  QString zoomOutSvg = QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'><path d='M3 8h10v1H3z'/></svg>");
  QIcon zoomOutIcon;
  zoomOutIcon.addPixmap(createSvgPixmap(zoomOutSvg.arg(normalColor), 16, 16), QIcon::Normal);
  zoomOutIcon.addPixmap(createSvgPixmap(zoomOutSvg.arg(hoverColor), 16, 16), QIcon::Active);
  
  QString zoomInSvg = QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'><path d='M8 3v5H3v1h5v5h1V9h5V8H9V3H8z'/></svg>");
  QIcon zoomInIcon;
  zoomInIcon.addPixmap(createSvgPixmap(zoomInSvg.arg(normalColor), 16, 16), QIcon::Normal);
  zoomInIcon.addPixmap(createSvgPixmap(zoomInSvg.arg(hoverColor), 16, 16), QIcon::Active);
  
  m_consoleZoomOutButton = createZoomButton(zoomOutIcon, "Zoom Out", consoleToolbar);
  m_consoleZoomInButton = createZoomButton(zoomInIcon, "Zoom In", consoleToolbar);
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
      "  background: rgba(255, 165, 0, 0.1); "
      "}"
      "QPushButton:checked { "
      "  background: rgba(255, 165, 0, 0.25); "
      "  border-radius: 2px; "
      "}"));
  m_guiLogAutoScrollButton->setToolTip("Auto-scroll");
  m_guiLogAutoScrollButton->setFocusPolicy(Qt::NoFocus);
  m_guiLogAutoScrollButton->setVisible(false);
  
  m_guiLogZoomOutButton = createZoomButton(zoomOutIcon, "Zoom Out", consoleToolbar);
  m_guiLogZoomOutButton->setVisible(false);
  
  m_guiLogZoomInButton = createZoomButton(zoomInIcon, "Zoom In", consoleToolbar);
  m_guiLogZoomInButton->setVisible(false);
  
  toolbarLayout->addWidget(m_autoScrollButton);
  toolbarLayout->addWidget(m_guiLogAutoScrollButton);
  toolbarLayout->addSpacing(5);
  toolbarLayout->addWidget(m_consoleZoomOutButton);
  toolbarLayout->addWidget(m_consoleZoomInButton);
  toolbarLayout->addWidget(m_guiLogZoomOutButton);
  toolbarLayout->addWidget(m_guiLogZoomInButton);
  
  m_consoleStack = new QStackedWidget(m_consoleContainer);
  
  m_outputDisplay = new QTextEdit(m_beamLogContainer);
  m_outputDisplay->setReadOnly(true);
  m_outputDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_outputDisplay->setStyleSheet(StyleManager::consoleOutput());
  m_beamLogLayout->addWidget(m_outputDisplay);
  
  m_guiLogContainer = new QWidget();
  m_guiLogLayout = new QVBoxLayout(m_guiLogContainer);
  m_guiLogLayout->setContentsMargins(0, 0, 0, 0);
  m_guiLogLayout->setSpacing(0);
  
  m_guiLogDisplay = new QTextEdit(m_guiLogContainer);
  m_guiLogDisplay->setReadOnly(true);
  m_guiLogDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_guiLogDisplay->setStyleSheet(StyleManager::consoleOutput());
  m_guiLogLayout->addWidget(m_guiLogDisplay);
  
  m_consoleStack->addWidget(m_beamLogContainer);
  m_consoleStack->addWidget(m_guiLogContainer);
  m_consoleStack->setCurrentIndex(0);
  
  consoleMainLayout->addWidget(consoleToolbar);
  consoleMainLayout->addWidget(m_consoleStack);
  
  connect(m_beamLogTabButton, &QPushButton::clicked, this, &DebugPane::showBeamLog);
  connect(m_guiLogTabButton, &QPushButton::clicked, this, &DebugPane::showGuiLog);
  
  connect(m_autoScrollButton, &QPushButton::toggled, this, &DebugPane::handleAutoScrollToggled);
  connect(m_consoleZoomInButton, &QPushButton::clicked, this, &DebugPane::handleConsoleZoomIn);
  connect(m_consoleZoomOutButton, &QPushButton::clicked, this, &DebugPane::handleConsoleZoomOut);
  
  connect(m_guiLogAutoScrollButton, &QPushButton::toggled, [this](bool checked) {
    m_guiLogAutoScroll = checked;
    if (checked) {
      QScrollBar *scrollBar = m_guiLogDisplay->verticalScrollBar();
      scrollBar->setValue(scrollBar->maximum());
    }
  });
  connect(m_guiLogZoomInButton, &QPushButton::clicked, this, &DebugPane::handleGuiLogZoomIn);
  connect(m_guiLogZoomOutButton, &QPushButton::clicked, this, &DebugPane::handleGuiLogZoomOut);
  
}

void DebugPane::setupDevTools()
{
  m_devToolsMainContainer = new QWidget(this);
  QVBoxLayout *devToolsMainLayout = new QVBoxLayout(m_devToolsMainContainer);
  devToolsMainLayout->setContentsMargins(0, 0, 0, 0);
  devToolsMainLayout->setSpacing(0);
  
  QWidget *devToolsToolbar = createTabToolbar(m_devToolsMainContainer);
  
  QHBoxLayout *toolbarLayout = new QHBoxLayout(devToolsToolbar);
  toolbarLayout->setContentsMargins(5, 2, 10, 2);
  toolbarLayout->setSpacing(2);
  
  m_devToolsTabButton = createTabButton("Dev Tools", devToolsToolbar);
  m_devToolsTabButton->setChecked(true);
  
  m_liveDashboardTabButton = createTabButton("Live Dashboard", devToolsToolbar);
  
  toolbarLayout->addWidget(m_devToolsTabButton);
  toolbarLayout->addWidget(m_liveDashboardTabButton);
  toolbarLayout->addStretch();
  
  QString devNormalColor = StyleManager::Colors::PRIMARY_ORANGE;
  QString devHoverColor = StyleManager::Colors::WHITE;
  QString devZoomOutSvg = QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'><path d='M3 8h10v1H3z'/></svg>");
  QIcon devZoomOutIcon;
  devZoomOutIcon.addPixmap(createSvgPixmap(devZoomOutSvg.arg(devNormalColor), 16, 16), QIcon::Normal);
  devZoomOutIcon.addPixmap(createSvgPixmap(devZoomOutSvg.arg(devHoverColor), 16, 16), QIcon::Active);
  
  QString devZoomInSvg = QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'><path d='M8 3v5H3v1h5v5h1V9h5V8H9V3H8z'/></svg>");
  QIcon devZoomInIcon;
  devZoomInIcon.addPixmap(createSvgPixmap(devZoomInSvg.arg(devNormalColor), 16, 16), QIcon::Normal);
  devZoomInIcon.addPixmap(createSvgPixmap(devZoomInSvg.arg(devHoverColor), 16, 16), QIcon::Active);
  
  m_zoomOutButton = createZoomButton(devZoomOutIcon, "Zoom Out", devToolsToolbar);
  m_zoomOutButton->setVisible(true);
  
  m_zoomInButton = createZoomButton(devZoomInIcon, "Zoom In", devToolsToolbar);
  m_zoomInButton->setVisible(true);
  
  m_liveDashboardZoomOutButton = createZoomButton(devZoomOutIcon, "Zoom Out", devToolsToolbar);
  m_liveDashboardZoomOutButton->setVisible(false);
  
  m_liveDashboardZoomInButton = createZoomButton(devZoomInIcon, "Zoom In", devToolsToolbar);
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
  
  m_devToolsView = new QWebEngineView(m_devToolsContainer);
  QWebEngineSettings *settings = m_devToolsView->page()->settings();
  settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
  settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
  m_devToolsView->page()->setBackgroundColor(QColor("#1e1e1e"));
  devToolsLayout->addWidget(m_devToolsView);
  
  m_liveDashboardContainer = new QWidget();
  QVBoxLayout *liveDashboardLayout = new QVBoxLayout(m_liveDashboardContainer);
  liveDashboardLayout->setContentsMargins(0, 0, 0, 0);
  liveDashboardLayout->setSpacing(0);
  
  m_liveDashboardView = new QWebEngineView(m_liveDashboardContainer);
  QWebEngineSettings *dashboardSettings = m_liveDashboardView->page()->settings();
  dashboardSettings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
  dashboardSettings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
  m_liveDashboardView->page()->setBackgroundColor(QColor("#1e1e1e"));
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
  
  connect(m_liveDashboardZoomInButton, &QPushButton::clicked, [this]() {
    if (m_liveDashboardView) {
      qreal currentZoom = m_liveDashboardView->zoomFactor();
      m_liveDashboardView->setZoomFactor(currentZoom * 1.1);
    }
  });
  
  connect(m_liveDashboardZoomOutButton, &QPushButton::clicked, [this]() {
    if (m_liveDashboardView) {
      qreal currentZoom = m_liveDashboardView->zoomFactor();
      m_liveDashboardView->setZoomFactor(currentZoom / 1.1);
    }
  });
  
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
      
      connect(m_devToolsView->page(), &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (ok) {
          applyDevToolsDarkTheme();
        }
      });
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

  QWidget *fullViewContainer = qobject_cast<QWidget*>(m_mainLayout->itemAt(1)->widget());
  if (!fullViewContainer) return;
  
  QVBoxLayout *fullViewLayout = qobject_cast<QVBoxLayout*>(fullViewContainer->layout());
  if (!fullViewLayout) return;
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

  resize(parentWidth, paneHeight);

  if (show)
  {
    move(0, parentHeight);
    QWidget::show();
    raise();
    m_slideAnimation->setStartValue(parentHeight);
    m_slideAnimation->setEndValue(parentHeight - paneHeight);
  }
  else
  {
    m_slideAnimation->setStartValue(parentHeight - height());
    m_slideAnimation->setEndValue(parentHeight);
  }

  m_slideAnimation->start();
  m_isVisible = show;
}

void DebugPane::handleAutoScrollToggled(bool checked)
{
  m_autoScroll = checked;
}

void DebugPane::handleZoomIn()
{
  if (m_devToolsView)
  {
    qreal currentZoom = m_devToolsView->zoomFactor();
    m_devToolsView->setZoomFactor(currentZoom + 0.1);
  }
}

void DebugPane::handleZoomOut()
{
  if (m_devToolsView)
  {
    qreal currentZoom = m_devToolsView->zoomFactor();
    if (currentZoom > 0.5)
    {
      m_devToolsView->setZoomFactor(currentZoom - 0.1);
    }
  }
}

void DebugPane::handleConsoleZoomIn()
{
  if (m_outputDisplay && m_currentFontSize < 24)
  {
    m_currentFontSize += 2;
    QFont font = m_outputDisplay->font();
    font.setPixelSize(m_currentFontSize);
    m_outputDisplay->setFont(font);
    m_outputDisplay->document()->setDefaultFont(font);
  }
}

void DebugPane::handleConsoleZoomOut()
{
  if (m_outputDisplay && m_currentFontSize > 8)
  {
    m_currentFontSize -= 2;
    QFont font = m_outputDisplay->font();
    font.setPixelSize(m_currentFontSize);
    m_outputDisplay->setFont(font);
    m_outputDisplay->document()->setDefaultFont(font);
  }
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
      event->position().y() < RESIZE_HANDLE_HEIGHT)
  {
    m_isResizing = true;
    m_resizeStartY = event->globalPosition().y();
    m_resizeStartHeight = height();
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
    int minHeight = 100;
    int maxHeight = parentWidget() ? parentWidget()->height() * 0.8 : 600;
    newHeight = qBound(minHeight, newHeight, maxHeight);

    resize(width(), newHeight);
    move(x(), parentWidget()->height() - newHeight);
    event->accept();
  }
  else if (event->position().y() < RESIZE_HANDLE_HEIGHT)
  {
    setCursor(Qt::SizeVerCursor);
  }
  else
  {
    setCursor(Qt::ArrowCursor);
  }

  QWidget::mouseMoveEvent(event);
}

void DebugPane::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton && m_isResizing)
  {
    m_isResizing = false;
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
  }
  QWidget::leaveEvent(event);
}

void DebugPane::applyDevToolsDarkTheme()
{
  QString darkModeCSS = R"(
    (function() {
      const style = document.createElement('style');
      style.textContent = `
        :root {
          filter: invert(1) hue-rotate(180deg);
          background: #1e1e1e !important;
        }
        
        img, svg, video, canvas, embed, object,
        .cm-color-swatch, .color-swatch {
          filter: invert(1) hue-rotate(180deg);
        }
        
        .cm-s-default .cm-keyword { filter: invert(1) hue-rotate(180deg); }
        .cm-s-default .cm-string { filter: invert(1) hue-rotate(180deg); }
        .cm-s-default .cm-number { filter: invert(1) hue-rotate(180deg); }
      `;
      document.head.appendChild(style);
    })();
  )";
  
  m_devToolsView->page()->runJavaScript(darkModeCSS);
}

void DebugPane::applyLiveDashboardTau5Theme()
{
  // Load CSS from resource file
  QFile cssFile(":/styles/tau5-dashboard-theme.css");
  QString cssContent;
  
  if (cssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream stream(&cssFile);
    cssContent = stream.readAll();
    cssFile.close();
  } else {
    qDebug() << "Failed to load tau5-dashboard-theme.css";
    return;
  }
  
  // Escape the CSS content for JavaScript
  cssContent.replace("\\", "\\\\");
  cssContent.replace("`", "\\`");
  cssContent.replace("$", "\\$");
  
  QString tau5CSS = QString(R"TAU5(
    (function() {
      const style = document.createElement('style');
      style.textContent = `%1`;
      document.head.appendChild(style);
    })();
  )TAU5").arg(cssContent);
  
  if (m_liveDashboardView && m_liveDashboardView->page()) {
    m_liveDashboardView->page()->runJavaScript(tau5CSS);
  }
}

QIcon DebugPane::createSvgIcon(const QString &normalSvg, const QString &hoverSvg, const QString &selectedSvg)
{
  QIcon icon;

  QByteArray normalBytes = normalSvg.toUtf8();
  QPixmap normalPixmap(32, 32);
  normalPixmap.fill(Qt::transparent);
  QSvgRenderer normalRenderer(normalBytes);
  if (normalRenderer.isValid())
  {
    QPainter painter(&normalPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    normalRenderer.render(&painter);
  }
  icon.addPixmap(normalPixmap, QIcon::Normal, QIcon::Off);

  if (!selectedSvg.isEmpty())
  {
    QByteArray selectedBytes = selectedSvg.toUtf8();
    QPixmap selectedPixmap(32, 32);
    selectedPixmap.fill(Qt::transparent);
    QSvgRenderer selectedRenderer(selectedBytes);
    if (selectedRenderer.isValid())
    {
      QPainter painter(&selectedPixmap);
      painter.setRenderHint(QPainter::Antialiasing);
      selectedRenderer.render(&painter);
    }
    icon.addPixmap(selectedPixmap, QIcon::Normal, QIcon::On);
  }
  
  return icon;
}

QPixmap DebugPane::createSvgPixmap(const QString &svg, int width, int height)
{
  QByteArray svgBytes = svg.toUtf8();
  QPixmap pixmap(width, height);
  pixmap.fill(Qt::transparent);
  QSvgRenderer renderer(svgBytes);
  if (renderer.isValid())
  {
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
  }
  return pixmap;
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
    m_liveDashboardView->setUrl(QUrl(url));
    
    // Connect to page load finished to apply tau5 theme
    connect(m_liveDashboardView->page(), &QWebEnginePage::loadFinished, this, [this](bool ok) {
      if (ok) {
        applyLiveDashboardTau5Theme();
      }
    });
  }
}

void DebugPane::handleGuiLogZoomIn()
{
  if (m_guiLogDisplay && m_guiLogFontSize < 24)
  {
    m_guiLogFontSize += 2;
    QFont font = m_guiLogDisplay->font();
    font.setPixelSize(m_guiLogFontSize);
    m_guiLogDisplay->setFont(font);
    m_guiLogDisplay->document()->setDefaultFont(font);
  }
}

void DebugPane::handleGuiLogZoomOut()
{
  if (m_guiLogDisplay && m_guiLogFontSize > 8)
  {
    m_guiLogFontSize -= 2;
    QFont font = m_guiLogDisplay->font();
    font.setPixelSize(m_guiLogFontSize);
    m_guiLogDisplay->setFont(font);
    m_guiLogDisplay->document()->setDefaultFont(font);
  }
}

void DebugPane::showBeamLog()
{
  m_beamLogTabButton->setChecked(true);
  m_guiLogTabButton->setChecked(false);
  m_consoleStack->setCurrentIndex(0);
  
  m_autoScrollButton->setVisible(true);
  m_consoleZoomOutButton->setVisible(true);
  m_consoleZoomInButton->setVisible(true);
  
  m_guiLogAutoScrollButton->setVisible(false);
  m_guiLogZoomOutButton->setVisible(false);
  m_guiLogZoomInButton->setVisible(false);
}

void DebugPane::showGuiLog()
{
  m_beamLogTabButton->setChecked(false);
  m_guiLogTabButton->setChecked(true);
  m_consoleStack->setCurrentIndex(1);
  
  m_autoScrollButton->setVisible(false);
  m_consoleZoomOutButton->setVisible(false);
  m_consoleZoomInButton->setVisible(false);
  
  m_guiLogAutoScrollButton->setVisible(true);
  m_guiLogZoomOutButton->setVisible(true);
  m_guiLogZoomInButton->setVisible(true);
}

void DebugPane::showDevToolsTab()
{
  m_devToolsTabButton->setChecked(true);
  m_liveDashboardTabButton->setChecked(false);
  m_devToolsStack->setCurrentIndex(0);
  
  m_zoomOutButton->setVisible(true);
  m_zoomInButton->setVisible(true);
  
  m_liveDashboardZoomOutButton->setVisible(false);
  m_liveDashboardZoomInButton->setVisible(false);
}

void DebugPane::showLiveDashboardTab()
{
  m_devToolsTabButton->setChecked(false);
  m_liveDashboardTabButton->setChecked(true);
  m_devToolsStack->setCurrentIndex(1);
  
  m_zoomOutButton->setVisible(false);
  m_zoomInButton->setVisible(false);
  
  m_liveDashboardZoomOutButton->setVisible(true);
  m_liveDashboardZoomInButton->setVisible(true);
  
  // Apply tau5 theme when switching to LiveDashboard tab
  if (m_liveDashboardView && m_liveDashboardView->page()) {
    applyLiveDashboardTau5Theme();
  }
}

QWidget* DebugPane::createTabToolbar(QWidget *parent)
{
  QWidget *toolbar = new QWidget(parent);
  toolbar->setFixedHeight(26);
  toolbar->setStyleSheet(QString(
      "QWidget { "
      "  background-color: %1; "
      "  border-bottom: 1px solid %2; "
      "}")
      .arg(StyleManager::Colors::blackAlpha(230))
      .arg(StyleManager::Colors::primaryOrangeAlpha(50)));
  
  return toolbar;
}

QString DebugPane::getTabButtonStyle()
{
  return QString(
      "QPushButton { "
      "  background: transparent; "
      "  color: %1; "
      "  border: none; "
      "  padding: 2px 8px; "
      "  font-family: %2; "
      "  font-size: %3; "
      "  font-weight: %4; "
      "} "
      "QPushButton:hover { "
      "  background: rgba(255, 165, 0, 0.1); "
      "} "
      "QPushButton:checked { "
      "  background: rgba(255, 165, 0, 0.2); "
      "  color: %5; "
      "}")
      .arg(StyleManager::Colors::primaryOrangeAlpha(180))
      .arg(StyleManager::Typography::MONOSPACE_FONT_FAMILY)
      .arg(StyleManager::Typography::FONT_SIZE_SMALL)
      .arg(StyleManager::Typography::FONT_WEIGHT_BOLD)
      .arg(StyleManager::Colors::PRIMARY_ORANGE);
}

QString DebugPane::getZoomButtonStyle()
{
  return QString(
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
      "  background: rgba(255, 165, 0, 0.1); "
      "} "
      "QPushButton:pressed { "
      "  background: rgba(255, 165, 0, 0.15); "
      "}");
}

QPushButton* DebugPane::createTabButton(const QString &text, QWidget *parent)
{
  QPushButton *button = new QPushButton(text, parent);
  button->setCheckable(true);
  button->setStyleSheet(getTabButtonStyle());
  button->setFocusPolicy(Qt::NoFocus);
  return button;
}

QPushButton* DebugPane::createZoomButton(const QIcon &icon, const QString &tooltip, QWidget *parent)
{
  QPushButton *button = new QPushButton(parent);
  button->setIcon(icon);
  button->setStyleSheet(getZoomButtonStyle());
  button->setToolTip(tooltip);
  button->setFocusPolicy(Qt::NoFocus);
  return button;
}

void DebugPane::saveSettings()
{
  QSettings settings;
  settings.beginGroup("DebugPane");
  
  settings.setValue("visible", m_isVisible);
  settings.setValue("height", height());
  settings.setValue("viewMode", static_cast<int>(m_currentMode));
  
  if (m_currentMode == SideBySide && m_splitter) {
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
  
  if (settings.contains("height")) {
    int savedHeight = settings.value("height", parentWidget()->height() / 2).toInt();
    resize(width(), savedHeight);
    if (parentWidget()) {
      move(0, parentWidget()->height() - savedHeight);
    }
  }
  
  if (settings.contains("viewMode")) {
    ViewMode savedMode = static_cast<ViewMode>(settings.value("viewMode", BeamLogOnly).toInt());
    setViewMode(savedMode);
  }
  
  if (m_currentMode == SideBySide && m_splitter && settings.contains("splitterSizes")) {
    m_splitter->restoreState(settings.value("splitterSizes").toByteArray());
  }
  
  if (settings.contains("beamLogFontSize")) {
    m_currentFontSize = settings.value("beamLogFontSize", 12).toInt();
    QFont font = m_outputDisplay->font();
    font.setPixelSize(m_currentFontSize);
    m_outputDisplay->setFont(font);
    m_outputDisplay->document()->setDefaultFont(font);
  }
  
  if (settings.contains("guiLogFontSize")) {
    m_guiLogFontSize = settings.value("guiLogFontSize", 12).toInt();
    QFont font = m_guiLogDisplay->font();
    font.setPixelSize(m_guiLogFontSize);
    m_guiLogDisplay->setFont(font);
    m_guiLogDisplay->document()->setDefaultFont(font);
  }
  
  m_autoScroll = settings.value("beamLogAutoScroll", true).toBool();
  m_autoScrollButton->setChecked(m_autoScroll);
  
  m_guiLogAutoScroll = settings.value("guiLogAutoScroll", true).toBool();
  m_guiLogAutoScrollButton->setChecked(m_guiLogAutoScroll);
  
  if (settings.contains("consoleTabIndex")) {
    int consoleIndex = settings.value("consoleTabIndex", 0).toInt();
    if (consoleIndex == 0) {
      showBeamLog();
    } else {
      showGuiLog();
    }
  }
  
  if (settings.contains("devToolsTabIndex")) {
    int devToolsIndex = settings.value("devToolsTabIndex", 0).toInt();
    if (devToolsIndex == 0) {
      showDevToolsTab();
    } else {
      showLiveDashboardTab();
    }
  }
  
  bool wasVisible = settings.value("visible", false).toBool();
  if (wasVisible) {
    m_isVisible = false;
  }
  
  settings.endGroup();
}

#include "moc_debugpane.cpp"

