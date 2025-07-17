// Icons used in this file are inspired by Microsoft VS Code Icons
// https://github.com/microsoft/vscode-icons
// Licensed under CC BY 4.0: https://creativecommons.org/licenses/by/4.0/
#include "debugpane.h"
#include "StyleManager.h"
#include "phxwebview.h"
#include "sandboxedwebview.h"
#include "../logger.h"
#include "../lib/fontloader.h"
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

// Custom splitter class to handle hover effects
class CustomSplitter : public QSplitter
{
public:
    CustomSplitter(Qt::Orientation orientation, QWidget *parent = nullptr) 
        : QSplitter(orientation, parent) {}
    
protected:
    QSplitterHandle *createHandle() override
    {
        return new CustomSplitterHandle(orientation(), this);
    }
    
private:
    class CustomSplitterHandle : public QSplitterHandle
    {
    public:
        CustomSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
            : QSplitterHandle(orientation, parent), m_isHovered(false)
        {
            setMouseTracking(true);
        }
        
    protected:
        void paintEvent(QPaintEvent *event) override
        {
            Q_UNUSED(event);
            QPainter painter(this);
            
            if (m_isHovered)
            {
                if (orientation() == Qt::Horizontal)
                {
                    // For horizontal splitter, draw vertical bar
                    int centerStart = (width() - DebugPane::RESIZE_HANDLE_VISUAL_HEIGHT) / 2;
                    painter.fillRect(centerStart, 0, DebugPane::RESIZE_HANDLE_VISUAL_HEIGHT, height(), 
                                   QColor(StyleManager::Colors::PRIMARY_ORANGE));
                }
                else
                {
                    // For vertical splitter, draw horizontal bar
                    int centerStart = (height() - DebugPane::RESIZE_HANDLE_VISUAL_HEIGHT) / 2;
                    painter.fillRect(0, centerStart, width(), DebugPane::RESIZE_HANDLE_VISUAL_HEIGHT, 
                                   QColor(StyleManager::Colors::PRIMARY_ORANGE));
                }
            }
        }
        
        void enterEvent(QEnterEvent *event) override
        {
            Q_UNUSED(event);
            m_isHovered = true;
            update();
        }
        
        void leaveEvent(QEvent *event) override
        {
            Q_UNUSED(event);
            m_isHovered = false;
            update();
        }
        
    private:
        bool m_isHovered;
    };
};

DebugPane::DebugPane(QWidget *parent)
    : QWidget(parent), m_isVisible(false), m_autoScroll(true), m_guiLogAutoScroll(true),
      m_maxLines(5000), m_currentMode(BeamLogOnly), m_isResizing(false), 
      m_resizeStartY(0), m_resizeStartHeight(0), m_isHoveringHandle(false),
      m_targetWebView(nullptr), m_devToolsView(nullptr), m_liveDashboardView(nullptr),
      m_iexShellView(nullptr), m_iexShellTabButton(nullptr),
      m_currentFontSize(12), m_guiLogFontSize(12), m_devToolsMainContainer(nullptr), 
      m_devToolsStack(nullptr), m_devToolsTabButton(nullptr), m_liveDashboardTabButton(nullptr),
      m_dragHandleWidget(nullptr)
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
  
  m_dragHandleWidget = new QWidget(this);
  m_dragHandleWidget->setFixedHeight(RESIZE_HANDLE_VISUAL_HEIGHT);
  m_dragHandleWidget->setMouseTracking(true);
  m_dragHandleWidget->hide();
  m_dragHandleWidget->setStyleSheet(QString(
      "background-color: %1;"
      ).arg(StyleManager::Colors::PRIMARY_ORANGE));

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
  m_headerLayout->setContentsMargins(10, 2, 10, 2);


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
      "  padding: 2px; "
      "  margin: 0 2px; "
      "  min-width: 24px; "
      "  max-width: 24px; "
      "  min-height: 16px; "
      "  max-height: 16px; "
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
  
  m_iexShellTabButton = createTabButton("Elixir", consoleToolbar);
  
  toolbarLayout->addWidget(m_beamLogTabButton);
  toolbarLayout->addWidget(m_guiLogTabButton);
  toolbarLayout->addWidget(m_iexShellTabButton);
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
  
  m_iexShellZoomOutButton = createZoomButton(zoomOutIcon, "Zoom Out", consoleToolbar);
  m_iexShellZoomOutButton->setVisible(false);
  
  m_iexShellZoomInButton = createZoomButton(zoomInIcon, "Zoom In", consoleToolbar);
  m_iexShellZoomInButton->setVisible(false);
  
  toolbarLayout->addWidget(m_autoScrollButton);
  toolbarLayout->addWidget(m_guiLogAutoScrollButton);
  toolbarLayout->addSpacing(5);
  toolbarLayout->addWidget(m_consoleZoomOutButton);
  toolbarLayout->addWidget(m_consoleZoomInButton);
  toolbarLayout->addWidget(m_guiLogZoomOutButton);
  toolbarLayout->addWidget(m_guiLogZoomInButton);
  toolbarLayout->addWidget(m_iexShellZoomOutButton);
  toolbarLayout->addWidget(m_iexShellZoomInButton);
  
  m_consoleStack = new QStackedWidget(m_consoleContainer);
  
  m_outputDisplay = new QTextEdit(m_beamLogContainer);
  m_outputDisplay->setReadOnly(true);
  m_outputDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_outputDisplay->setStyleSheet(StyleManager::consoleOutput());
  
  QFont consoleFont("Cascadia Code PL", 10);
  consoleFont.setStyleHint(QFont::Monospace);
  consoleFont.setPixelSize(m_currentFontSize);
  m_outputDisplay->setFont(consoleFont);
  m_outputDisplay->document()->setDefaultFont(consoleFont);
  
  m_beamLogLayout->addWidget(m_outputDisplay);
  
  m_guiLogContainer = new QWidget();
  m_guiLogLayout = new QVBoxLayout(m_guiLogContainer);
  m_guiLogLayout->setContentsMargins(0, 0, 0, 0);
  m_guiLogLayout->setSpacing(0);
  
  m_guiLogDisplay = new QTextEdit(m_guiLogContainer);
  m_guiLogDisplay->setReadOnly(true);
  m_guiLogDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_guiLogDisplay->setStyleSheet(StyleManager::consoleOutput());
  
  QFont guiLogFont("Cascadia Code PL", 10);
  guiLogFont.setStyleHint(QFont::Monospace);
  guiLogFont.setPixelSize(m_guiLogFontSize);
  m_guiLogDisplay->setFont(guiLogFont);
  m_guiLogDisplay->document()->setDefaultFont(guiLogFont);
  
  m_guiLogLayout->addWidget(m_guiLogDisplay);
  
  // Create Elixir console container
  m_iexShellContainer = new QWidget();
  QVBoxLayout *iexShellLayout = new QVBoxLayout(m_iexShellContainer);
  iexShellLayout->setContentsMargins(0, 0, 0, 0);
  iexShellLayout->setSpacing(0);
  
  m_iexShellView = new SandboxedWebView(m_iexShellContainer);
  // Don't set fallback URL here - it will be set in setIexShellUrl with token
  m_iexShellView->page()->setBackgroundColor(QColor("#000000"));
  // Don't load URL here - wait for setIexShellUrl to be called with token
  iexShellLayout->addWidget(m_iexShellView);
  
  m_consoleStack->addWidget(m_beamLogContainer);
  m_consoleStack->addWidget(m_guiLogContainer);
  m_consoleStack->addWidget(m_iexShellContainer);
  m_consoleStack->setCurrentIndex(0);
  
  consoleMainLayout->addWidget(consoleToolbar);
  consoleMainLayout->addWidget(m_consoleStack);
  
  connect(m_beamLogTabButton, &QPushButton::clicked, this, &DebugPane::showBeamLog);
  connect(m_guiLogTabButton, &QPushButton::clicked, this, &DebugPane::showGuiLog);
  connect(m_iexShellTabButton, &QPushButton::clicked, this, &DebugPane::showIexShell);
  
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
  
  connect(m_iexShellZoomInButton, &QPushButton::clicked, this, &DebugPane::handleIexShellZoomIn);
  connect(m_iexShellZoomOutButton, &QPushButton::clicked, this, &DebugPane::handleIexShellZoomOut);
  
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
  
  m_devToolsView = new SandboxedWebView(m_devToolsContainer);
  m_devToolsView->setFallbackUrl(QUrl());  // No fallback for DevTools
  m_devToolsView->page()->setBackgroundColor(QColor("#1e1e1e"));
  
  // Configure Qt WebEngine font settings for fixed-width fonts
  QWebEngineSettings* devToolsSettings = m_devToolsView->settings();
  devToolsSettings->setFontFamily(QWebEngineSettings::FixedFont, "Cascadia Code");
  devToolsSettings->setFontSize(QWebEngineSettings::DefaultFixedFontSize, 14);
  
  // Inject persistent font override script for DevTools
  injectDevToolsFontScript();
  
  devToolsLayout->addWidget(m_devToolsView);
  
  m_liveDashboardContainer = new QWidget();
  QVBoxLayout *liveDashboardLayout = new QVBoxLayout(m_liveDashboardContainer);
  liveDashboardLayout->setContentsMargins(0, 0, 0, 0);
  liveDashboardLayout->setSpacing(0);
  
  m_liveDashboardView = new SandboxedWebView(m_liveDashboardContainer);
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
      
      injectDevToolsFontScript();
      
      connect(m_devToolsView->page(), &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (ok) {
          applyDevToolsDarkTheme();
          injectDevToolsFontScript();
          emit webDevToolsLoaded();
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

  paneHeight = constrainHeight(paneHeight);
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
    QFont font("Cascadia Code PL", 10);
    font.setStyleHint(QFont::Monospace);
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
    QFont font("Cascadia Code PL", 10);
    font.setStyleHint(QFont::Monospace);
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
    // Check if hovering over the orange drag handle only
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

void DebugPane::applyDevToolsDarkTheme()
{
  QString darkModeCSS = R"(
    (function() {
      const style = document.createElement('style');
      style.textContent = `
        :root {
          filter: invert(1) hue-rotate(180deg);
          background: #1e1e1e !important;
          /* Try setting font via CSS variable */
          --monospace-font: 'Cascadia Code PL', 'Cascadia Code', 'Cascadia Mono', Consolas, 'Courier New', monospace !important;
        }
        
        img, svg, video, canvas, embed, object,
        .cm-color-swatch, .color-swatch {
          filter: invert(1) hue-rotate(180deg);
        }
        
        .cm-s-default .cm-keyword { filter: invert(1) hue-rotate(180deg); }
        .cm-s-default .cm-string { filter: invert(1) hue-rotate(180deg); }
        .cm-s-default .cm-number { filter: invert(1) hue-rotate(180deg); }
        
        /* Set Cascadia Code font ONLY for code and console elements */
        /* Note: Chrome DevTools often ignores font-family changes for security reasons */
        .console-message-text,
        .console-user-command,
        .console-user-command-result,
        .monospace,
        .source-code,
        .cm-s-default,
        .CodeMirror,
        .CodeMirror pre,
        .object-value-string,
        .object-value-number,
        .object-value-boolean,
        .object-value-null,
        .object-value-undefined,
        .object-value-function,
        .object-value-regexp,
        .console-formatted-string,
        .console-formatted-object,
        .console-formatted-node,
        .console-formatted-array,
        span.monospace,
        .webkit-css-property,
        .devtools-link[data-url],
        .console-message-wrapper .source-code {
          font-family: var(--monospace-font, 'SF Mono', 'Monaco', 'Menlo', 'Cascadia Code', 'Cascadia Mono', Consolas, 'Courier New', monospace) !important;
        }
      `;
      document.head.appendChild(style);
    })();
  )";
  
  m_devToolsView->page()->runJavaScript(darkModeCSS);
  
  QTimer::singleShot(500, this, [this]() {
    if (m_devToolsView && m_devToolsView->page()) {
      QString devToolsScrollbarCSS = R"(
        (function() {
          const style = document.createElement('style');
          style.setAttribute('id', 'tau5-devtools-scrollbar');
          style.textContent = `
            *::-webkit-scrollbar,
            body ::-webkit-scrollbar,
            .vbox ::-webkit-scrollbar,
            .widget ::-webkit-scrollbar,
            .console-view ::-webkit-scrollbar,
            .elements-tree-outline ::-webkit-scrollbar,
            .monospace ::-webkit-scrollbar,
            .source-code ::-webkit-scrollbar,
            .viewport ::-webkit-scrollbar,
            .scroller ::-webkit-scrollbar,
            ::-webkit-scrollbar {
              width: 8px !important;
              height: 8px !important;
              background: transparent !important;
              background-color: transparent !important;
            }
            
            *::-webkit-scrollbar-track,
            body ::-webkit-scrollbar-track,
            .vbox ::-webkit-scrollbar-track,
            .widget ::-webkit-scrollbar-track,
            .viewport ::-webkit-scrollbar-track,
            .scroller ::-webkit-scrollbar-track,
            ::-webkit-scrollbar-track {
              background: transparent !important;
              background-color: transparent !important;
              border: none !important;
              box-shadow: none !important;
            }
            
            *::-webkit-scrollbar-thumb,
            body ::-webkit-scrollbar-thumb,
            .vbox ::-webkit-scrollbar-thumb,
            .widget ::-webkit-scrollbar-thumb,
            .viewport ::-webkit-scrollbar-thumb,
            .scroller ::-webkit-scrollbar-thumb,
            ::-webkit-scrollbar-thumb {
              background: rgba(255, 165, 0, 0.941) !important;
              background-color: rgba(255, 165, 0, 0.941) !important;
              border-radius: 0px !important;
              min-height: 30px !important;
              border: none !important;
              margin: 0px !important;
              box-shadow: none !important;
            }
            
            *::-webkit-scrollbar-thumb:hover,
            body ::-webkit-scrollbar-thumb:hover,
            .vbox ::-webkit-scrollbar-thumb:hover,
            .widget ::-webkit-scrollbar-thumb:hover,
            ::-webkit-scrollbar-thumb:hover {
              background: rgba(255, 165, 0, 1.0) !important;
              background-color: rgba(255, 165, 0, 1.0) !important;
            }
            
            *::-webkit-scrollbar-corner,
            body ::-webkit-scrollbar-corner,
            ::-webkit-scrollbar-corner {
              background: transparent !important;
              background-color: transparent !important;
            }
            
            *::-webkit-scrollbar-button,
            body ::-webkit-scrollbar-button,
            ::-webkit-scrollbar-button {
              display: none !important;
              width: 0 !important;
              height: 0 !important;
            }
          `;
          
          const existing = document.getElementById('tau5-devtools-scrollbar');
          if (existing) {
            existing.remove();
          }
          document.head.appendChild(style);
          
          document.querySelectorAll('*').forEach(el => {
            if (el.shadowRoot) {
              const shadowStyle = style.cloneNode(true);
              el.shadowRoot.appendChild(shadowStyle);
            }
          });
        })();
      )";
      
      m_devToolsView->page()->runJavaScript(devToolsScrollbarCSS);
    }
  });
  
}

void DebugPane::applyLiveDashboardTau5Theme()
{
  // First inject the embedded font
  QString cascadiaCodeCss = FontLoader::getCascadiaCodeCss();
  if (!cascadiaCodeCss.isEmpty()) {
    // Escape the CSS for JavaScript
    cascadiaCodeCss.replace("\\", "\\\\");
    cascadiaCodeCss.replace("`", "\\`");
    cascadiaCodeCss.replace("$", "\\$");
    
    QString fontInjectScript = QString(R"(
      (function() {
        const fontStyle = document.createElement('style');
        fontStyle.id = 'tau5-cascadia-font-dashboard';
        fontStyle.textContent = `%1`;
        document.head.appendChild(fontStyle);
      })();
    )").arg(cascadiaCodeCss);
    
    if (m_liveDashboardView && m_liveDashboardView->page()) {
      m_liveDashboardView->page()->runJavaScript(fontInjectScript);
    }
  }
  
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
    
    // Apply shared scrollbar styling
    m_liveDashboardView->page()->runJavaScript(getDarkScrollbarCSS());
  }
}

void DebugPane::applyConsoleDarkTheme()
{
  if (m_iexShellView && m_iexShellView->page()) {
    m_iexShellView->page()->runJavaScript(getDarkScrollbarCSS());
    
    // Get the CSS with embedded base64 font
    QString cascadiaCodeCss = FontLoader::getCascadiaCodeCss();
    if (!cascadiaCodeCss.isEmpty()) {
      // Escape the CSS for JavaScript
      cascadiaCodeCss.replace("\\", "\\\\");
      cascadiaCodeCss.replace("`", "\\`");
      cascadiaCodeCss.replace("$", "\\$");
      
      // Inject the font CSS first
      QString fontInjectScript = QString(R"(
        (function() {
          const fontStyle = document.createElement('style');
          fontStyle.id = 'tau5-cascadia-font-console';
          fontStyle.textContent = `%1`;
          document.head.appendChild(fontStyle);
        })();
      )").arg(cascadiaCodeCss);
      
      m_iexShellView->page()->runJavaScript(fontInjectScript);
    }
    
    QString consoleCSS = R"(
      (function() {
        const style = document.createElement('style');
        style.textContent = `
          .tau5-terminal {
            display: flex;
            flex-direction: column;
            background-color: #000000;
            color: #ffffff;
            font-family: 'Cascadia Code PL', 'SF Mono', 'Monaco', 'Menlo', 'Cascadia Code', 'Cascadia Mono', Consolas, 'Courier New', monospace;
            font-size: 0.875rem;
            line-height: 1.25rem;
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            width: 100%;
            height: 100%;
            user-select: text;
          }
          
          .tau5-terminal-output {
            flex: 1 1 0%;
            overflow-y: auto;
            padding: 1rem;
            white-space: pre-wrap;
            word-wrap: break-word;
            user-select: text;
            -webkit-user-select: text;
            -moz-user-select: text;
            -ms-user-select: text;
          }
          
          .tau5-terminal-output pre {
            user-select: text;
            -webkit-user-select: text;
            -moz-user-select: text;
            -ms-user-select: text;
          }
          
          .tau5-input-line {
            display: flex;
            align-items: center;
            padding: 1rem;
            border-top: 1px solid #1f2937;
          }
          
          .tau5-prompt {
            color: #f97316;
            font-weight: 700;
            margin-right: 0.5rem;
            white-space: nowrap;
          }
          
          .tau5-terminal-input {
            background-color: transparent;
            border: none;
            outline: none;
            flex: 1 1 0%;
            color: #ffffff;
            font-family: inherit;
          }
          
          .tau5-atom { color: #FFA500; }
          .tau5-string { color: #00FF00; }
          .tau5-number { color: #4169E1; }
          .tau5-keyword { color: #FF00FF; }
          .tau5-module { color: #00FFFF; }
          .tau5-regex { color: #FF1493; }
          .tau5-output-error { color: #FF1493; }
          
          .tau5-access-denied {
            display: flex;
            align-items: center;
            justify-content: center;
          }
          
          .tau5-error-container {
            text-align: center;
            padding: 2rem;
          }
          
          .tau5-error-title {
            font-size: 1.875rem;
            line-height: 2.25rem;
            font-weight: 700;
            margin-bottom: 1rem;
            color: #FF1493;
          }
          
          .tau5-error-message {
            color: #9ca3af;
            margin-bottom: 1.5rem;
            white-space: pre-line;
          }
          
          .tau5-error-icon {
            font-size: 3.75rem;
            line-height: 1;
          }
          
          body:has(.tau5-terminal) main {
            padding: 0 !important;
            max-width: none !important;
            height: 100vh !important;
          }
          
          body:has(.tau5-terminal) main > div {
            max-width: none !important;
            height: 100% !important;
            position: relative;
          }
          
          .tau5-terminal-output::-webkit-scrollbar {
            width: 8px !important;
            height: 8px !important;
            background: transparent !important;
          }
          
          .tau5-terminal-output::-webkit-scrollbar-track {
            background: transparent !important;
            border: none !important;
          }
          
          .tau5-terminal-output::-webkit-scrollbar-thumb {
            background: rgba(255, 165, 0, 0.941) !important;
            border-radius: 0px !important;
            min-height: 30px !important;
            border: none !important;
            margin: 0px !important;
          }
          
          .tau5-terminal-output::-webkit-scrollbar-thumb:hover {
            background: rgba(255, 165, 0, 1.0) !important;
          }
        `;
        document.head.appendChild(style);
      })();
    )";
    
    m_iexShellView->page()->runJavaScript(consoleCSS);
  }
}

void DebugPane::injectDevToolsFontScript()
{
  if (!m_devToolsView || !m_devToolsView->page()) {
    return;
  }
  
  Logger::log(Logger::Debug, "Injecting Cascadia Code font into DevTools");
  
  // Get the CSS with embedded base64 font
  QString cascadiaCodeCss = FontLoader::getCascadiaCodeCss();
  if (cascadiaCodeCss.isEmpty()) {
    Logger::log(Logger::Warning, "Failed to load Cascadia Code font for DevTools");
    return;
  }
  
  Logger::log(Logger::Debug, QString("Cascadia Code CSS size: %1 characters").arg(cascadiaCodeCss.length()));
  
  // Escape the CSS for JavaScript
  cascadiaCodeCss.replace("\\", "\\\\");
  cascadiaCodeCss.replace("`", "\\`");
  cascadiaCodeCss.replace("$", "\\$");
  
  QWebEngineScript fontScript;
  fontScript.setName("CascadiaCodeFont");
  fontScript.setWorldId(QWebEngineScript::ApplicationWorld);
  fontScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
  fontScript.setRunsOnSubFrames(true);
  
  QString scriptSource = QString(R"SCRIPT(
    (function() {
      const observer = new MutationObserver(function(mutations) {
        const style = document.getElementById('tau5-cascadia-font') || document.createElement('style');
        style.id = 'tau5-cascadia-font';
        style.textContent = `%1`;
        
        if (!document.getElementById('tau5-cascadia-font')) {
          document.head.appendChild(style);
        }
        
        // Also inject into any shadow roots
        document.querySelectorAll('*').forEach(el => {
          if (el.shadowRoot && !el.shadowRoot.getElementById('tau5-cascadia-font-shadow')) {
            const shadowStyle = style.cloneNode(true);
            shadowStyle.id = 'tau5-cascadia-font-shadow';
            el.shadowRoot.appendChild(shadowStyle);
          }
        });
      });
      
      // Start observing
      observer.observe(document, {
        childList: true,
        subtree: true
      });
      
      // Also run immediately
      observer.callback = observer._callback;
      observer.callback([]);
    })();
  )SCRIPT").arg(cascadiaCodeCss);
  
  fontScript.setSourceCode(scriptSource);
  
  // Remove any existing font script and add the new one
  QWebEngineScriptCollection &scripts = m_devToolsView->page()->scripts();
  QList<QWebEngineScript> existingScripts = scripts.find("CascadiaCodeFont");
  for (const QWebEngineScript &script : existingScripts) {
    scripts.remove(script);
  }
  scripts.insert(fontScript);
}

QString DebugPane::getDarkScrollbarCSS() const
{
  return R"(
    (function() {
      const style = document.createElement('style');
      style.textContent = `
        *::-webkit-scrollbar,
        ::-webkit-scrollbar {
          width: 8px !important;
          height: 8px !important;
          background: transparent !important;
        }
        
        *::-webkit-scrollbar-track,
        ::-webkit-scrollbar-track {
          background: transparent !important;
          border: none !important;
        }
        
        *::-webkit-scrollbar-thumb,
        ::-webkit-scrollbar-thumb {
          background: rgba(255, 165, 0, 0.941) !important;
          border-radius: 0px !important;
          min-height: 30px !important;
          border: none !important;
          margin: 0px !important;
        }
        
        *::-webkit-scrollbar-thumb:hover,
        ::-webkit-scrollbar-thumb:hover {
          background: rgba(255, 165, 0, 1.0) !important;
        }
        
        *::-webkit-scrollbar-thumb:active,
        ::-webkit-scrollbar-thumb:active {
          background: rgba(255, 165, 0, 1.0) !important;
        }
        
        ::-webkit-scrollbar-corner {
          background: rgba(0, 0, 0, 0);
        }
        
        /* Hide scrollbar buttons */
        ::-webkit-scrollbar-button {
          height: 0px;
          background: rgba(0, 0, 0, 0);
          display: none;
        }
      `;
      document.head.appendChild(style);
    })();
  )";
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
    QUrl dashboardUrl(url);
    m_liveDashboardView->setFallbackUrl(dashboardUrl);
    m_liveDashboardView->setUrl(dashboardUrl);
    
    connect(m_liveDashboardView->page(), &QWebEnginePage::loadFinished, this, [this](bool ok) {
      if (ok) {
        applyLiveDashboardTau5Theme();
        emit liveDashboardLoaded();
      }
    });
  }
}

void DebugPane::setIexShellUrl(const QString &url)
{
  if (m_iexShellView && !url.isEmpty())
  {
    QUrl iexUrl(url);
    Logger::log(Logger::Debug, QString("DebugPane::setIexShellUrl - Setting URL: %1").arg(url));
    m_iexShellView->setFallbackUrl(iexUrl);
    m_iexShellView->load(iexUrl);
    
    connect(m_iexShellView->page(), &QWebEnginePage::loadFinished, this, [this](bool ok) {
      if (ok) {
        applyConsoleDarkTheme();
        emit elixirConsoleLoaded();
      }
    });
  } else {
    Logger::log(Logger::Warning, "DebugPane::setIexShellUrl - m_iexShellView is null or url is empty");
  }
}

void DebugPane::handleGuiLogZoomIn()
{
  if (m_guiLogDisplay && m_guiLogFontSize < 24)
  {
    m_guiLogFontSize += 2;
    QFont font("Cascadia Code PL", 10);
    font.setStyleHint(QFont::Monospace);
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
    QFont font("Cascadia Code PL", 10);
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(m_guiLogFontSize);
    m_guiLogDisplay->setFont(font);
    m_guiLogDisplay->document()->setDefaultFont(font);
  }
}

void DebugPane::handleIexShellZoomIn()
{
  if (m_iexShellView)
  {
    qreal currentZoom = m_iexShellView->zoomFactor();
    m_iexShellView->setZoomFactor(currentZoom * 1.1);
  }
}

void DebugPane::handleIexShellZoomOut()
{
  if (m_iexShellView)
  {
    qreal currentZoom = m_iexShellView->zoomFactor();
    m_iexShellView->setZoomFactor(currentZoom / 1.1);
  }
}

void DebugPane::showBeamLog()
{
  m_beamLogTabButton->setChecked(true);
  m_guiLogTabButton->setChecked(false);
  m_iexShellTabButton->setChecked(false);
  m_consoleStack->setCurrentIndex(0);
  
  m_autoScrollButton->setVisible(true);
  m_consoleZoomOutButton->setVisible(true);
  m_consoleZoomInButton->setVisible(true);
  
  m_guiLogAutoScrollButton->setVisible(false);
  m_guiLogZoomOutButton->setVisible(false);
  m_guiLogZoomInButton->setVisible(false);
  
  m_iexShellZoomOutButton->setVisible(false);
  m_iexShellZoomInButton->setVisible(false);
}

void DebugPane::showGuiLog()
{
  m_beamLogTabButton->setChecked(false);
  m_guiLogTabButton->setChecked(true);
  m_iexShellTabButton->setChecked(false);
  m_consoleStack->setCurrentIndex(1);
  
  m_autoScrollButton->setVisible(false);
  m_consoleZoomOutButton->setVisible(false);
  m_consoleZoomInButton->setVisible(false);
  
  m_guiLogAutoScrollButton->setVisible(true);
  m_guiLogZoomOutButton->setVisible(true);
  m_guiLogZoomInButton->setVisible(true);
  
  m_iexShellZoomOutButton->setVisible(false);
  m_iexShellZoomInButton->setVisible(false);
}

void DebugPane::showIexShell()
{
  m_beamLogTabButton->setChecked(false);
  m_guiLogTabButton->setChecked(false);
  m_iexShellTabButton->setChecked(true);
  m_consoleStack->setCurrentIndex(2);
  
  m_autoScrollButton->setVisible(false);
  m_consoleZoomOutButton->setVisible(false);
  m_consoleZoomInButton->setVisible(false);
  
  m_guiLogAutoScrollButton->setVisible(false);
  m_guiLogZoomOutButton->setVisible(false);
  m_guiLogZoomInButton->setVisible(false);
  
  m_iexShellZoomOutButton->setVisible(true);
  m_iexShellZoomInButton->setVisible(true);
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
    savedHeight = constrainHeight(savedHeight);
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
    QFont font("Cascadia Code PL", 10);
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(m_currentFontSize);
    m_outputDisplay->setFont(font);
    m_outputDisplay->document()->setDefaultFont(font);
  }
  
  if (settings.contains("guiLogFontSize")) {
    m_guiLogFontSize = settings.value("guiLogFontSize", 12).toInt();
    QFont font("Cascadia Code PL", 10);
    font.setStyleHint(QFont::Monospace);
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
    } else if (consoleIndex == 1) {
      showGuiLog();
    } else if (consoleIndex == 2) {
      showIexShell();
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

