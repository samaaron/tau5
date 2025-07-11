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

DebugPane::DebugPane(QWidget *parent)
    : QWidget(parent), m_isVisible(false), m_autoScroll(true), m_maxLines(5000),
      m_currentMode(BeamLogOnly), m_isResizing(false), m_resizeStartY(0),
      m_resizeStartHeight(0), m_targetWebView(nullptr), m_devToolsView(nullptr),
      m_currentFontSize(12)
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
  setupConsole();
  setupDevTools();

  // Create a simple container widget for full view mode
  QWidget *fullViewContainer = new QWidget(this);
  QVBoxLayout *fullViewLayout = new QVBoxLayout(fullViewContainer);
  fullViewLayout->setContentsMargins(0, 0, 0, 0);
  fullViewLayout->setSpacing(0);
  
  // Splitter for side-by-side view
  m_splitter = new QSplitter(Qt::Horizontal, this);
  
  // Create left and right placeholders for the splitter
  QWidget *leftPlaceholder = new QWidget();
  QWidget *rightPlaceholder = new QWidget();
  m_splitter->addWidget(leftPlaceholder);
  m_splitter->addWidget(rightPlaceholder);
  m_splitter->setSizes({1000, 1000});

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

  QLabel *titleLabel = new QLabel("Debug Pane", m_headerWidget);
  titleLabel->setStyleSheet(
      QString("QLabel { "
              "  color: %1; "
              "  background: transparent; "
              "  font-family: %2; "
              "  font-weight: %3; "
              "  font-size: %4; "
              "  letter-spacing: 1px; "
              "  padding: %5 %6; "
              "}")
          .arg(StyleManager::Colors::PRIMARY_ORANGE)
          .arg(StyleManager::Typography::MONOSPACE_FONT_FAMILY)
          .arg(StyleManager::Typography::FONT_WEIGHT_BOLD)
          .arg(StyleManager::Typography::FONT_SIZE_LARGE)
          .arg(StyleManager::Spacing::SMALL)
          .arg(StyleManager::Spacing::LARGE));

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

  m_headerLayout->addWidget(titleLabel);
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
  m_consoleLayout = new QVBoxLayout(m_consoleContainer);
  m_consoleLayout->setContentsMargins(0, 0, 0, 0);
  m_consoleLayout->setSpacing(0);

  // Console toolbar with auto-scroll control
  QWidget *consoleToolbar = new QWidget(m_consoleContainer);
  consoleToolbar->setFixedHeight(26);
  consoleToolbar->setStyleSheet(QString(
      "QWidget { "
      "  background-color: %1; "
      "  border-bottom: 1px solid %2; "
      "}")
      .arg(StyleManager::Colors::blackAlpha(230))
      .arg(StyleManager::Colors::primaryOrangeAlpha(50)));
  
  QHBoxLayout *toolbarLayout = new QHBoxLayout(consoleToolbar);
  toolbarLayout->setContentsMargins(10, 2, 10, 2);
  toolbarLayout->setSpacing(5);
  
  QLabel *consoleLabel = new QLabel("BEAM Log", consoleToolbar);
  consoleLabel->setStyleSheet(QString(
      "QLabel { "
      "  color: %1; "
      "  font-family: %2; "
      "  font-size: %3; "
      "  font-weight: %4; "
      "  background: transparent; "
      "}")
      .arg(StyleManager::Colors::PRIMARY_ORANGE)
      .arg(StyleManager::Typography::MONOSPACE_FONT_FAMILY)
      .arg(StyleManager::Typography::FONT_SIZE_SMALL)
      .arg(StyleManager::Typography::FONT_WEIGHT_BOLD));
  
  toolbarLayout->addWidget(consoleLabel);
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
  
  toolbarLayout->addWidget(m_autoScrollButton);
  toolbarLayout->addSpacing(5);
  QString zoomOutSvg = QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'><path d='M3 8h10v1H3z'/></svg>");
  QIcon zoomOutIcon;
  zoomOutIcon.addPixmap(createSvgPixmap(zoomOutSvg.arg(normalColor), 16, 16), QIcon::Normal);
  zoomOutIcon.addPixmap(createSvgPixmap(zoomOutSvg.arg(hoverColor), 16, 16), QIcon::Active);
  
  QString zoomInSvg = QString("<svg viewBox='0 0 16 16' fill='%1' xmlns='http://www.w3.org/2000/svg'><path d='M8 3v5H3v1h5v5h1V9h5V8H9V3H8z'/></svg>");
  QIcon zoomInIcon;
  zoomInIcon.addPixmap(createSvgPixmap(zoomInSvg.arg(normalColor), 16, 16), QIcon::Normal);
  zoomInIcon.addPixmap(createSvgPixmap(zoomInSvg.arg(hoverColor), 16, 16), QIcon::Active);
  
  QString zoomButtonStyle = QString(
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
  
  m_consoleZoomOutButton = new QPushButton(consoleToolbar);
  m_consoleZoomOutButton->setIcon(zoomOutIcon);
  m_consoleZoomOutButton->setStyleSheet(zoomButtonStyle);
  m_consoleZoomOutButton->setToolTip("Zoom Out");
  m_consoleZoomOutButton->setFocusPolicy(Qt::NoFocus);
  
  m_consoleZoomInButton = new QPushButton(consoleToolbar);
  m_consoleZoomInButton->setIcon(zoomInIcon);
  m_consoleZoomInButton->setStyleSheet(zoomButtonStyle);
  m_consoleZoomInButton->setToolTip("Zoom In");
  m_consoleZoomInButton->setFocusPolicy(Qt::NoFocus);
  
  toolbarLayout->addWidget(m_consoleZoomOutButton);
  toolbarLayout->addWidget(m_consoleZoomInButton);
  
  connect(m_autoScrollButton, &QPushButton::toggled, this, &DebugPane::handleAutoScrollToggled);
  connect(m_consoleZoomInButton, &QPushButton::clicked, this, &DebugPane::handleConsoleZoomIn);
  connect(m_consoleZoomOutButton, &QPushButton::clicked, this, &DebugPane::handleConsoleZoomOut);

  m_outputDisplay = new QTextEdit(m_consoleContainer);
  m_outputDisplay->setReadOnly(true);
  m_outputDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_outputDisplay->setStyleSheet(StyleManager::consoleOutput());

  m_consoleLayout->addWidget(consoleToolbar);
  m_consoleLayout->addWidget(m_outputDisplay);
}

void DebugPane::setupDevTools()
{
  // Create a container widget for devtools with toolbar
  m_devToolsContainer = new QWidget(this);
  QVBoxLayout *devToolsLayout = new QVBoxLayout(m_devToolsContainer);
  devToolsLayout->setContentsMargins(0, 0, 0, 0);
  devToolsLayout->setSpacing(0);
  
  QWidget *devToolsToolbar = new QWidget(m_devToolsContainer);
  devToolsToolbar->setFixedHeight(26);
  devToolsToolbar->setStyleSheet(QString(
      "QWidget { "
      "  background-color: %1; "
      "  border-bottom: 1px solid %2; "
      "}")
      .arg(StyleManager::Colors::blackAlpha(230))
      .arg(StyleManager::Colors::primaryOrangeAlpha(50)));
  
  QHBoxLayout *toolbarLayout = new QHBoxLayout(devToolsToolbar);
  toolbarLayout->setContentsMargins(10, 2, 10, 2);
  toolbarLayout->setSpacing(5);
  
  QLabel *devToolsLabel = new QLabel("Developer Tools", devToolsToolbar);
  devToolsLabel->setStyleSheet(QString(
      "QLabel { "
      "  color: %1; "
      "  font-family: %2; "
      "  font-size: %3; "
      "  font-weight: %4; "
      "  background: transparent; "
      "}")
      .arg(StyleManager::Colors::PRIMARY_ORANGE)
      .arg(StyleManager::Typography::MONOSPACE_FONT_FAMILY)
      .arg(StyleManager::Typography::FONT_SIZE_SMALL)
      .arg(StyleManager::Typography::FONT_WEIGHT_BOLD));
  
  toolbarLayout->addWidget(devToolsLabel);
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
  
  QString devZoomButtonStyle = QString(
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
  
  m_zoomOutButton = new QPushButton(devToolsToolbar);
  m_zoomOutButton->setIcon(devZoomOutIcon);
  m_zoomOutButton->setStyleSheet(devZoomButtonStyle);
  m_zoomOutButton->setToolTip("Zoom Out");
  m_zoomOutButton->setFocusPolicy(Qt::NoFocus);
  
  m_zoomInButton = new QPushButton(devToolsToolbar);
  m_zoomInButton->setIcon(devZoomInIcon);
  m_zoomInButton->setStyleSheet(devZoomButtonStyle);
  m_zoomInButton->setToolTip("Zoom In");
  m_zoomInButton->setFocusPolicy(Qt::NoFocus);
  
  toolbarLayout->addWidget(m_zoomOutButton);
  toolbarLayout->addWidget(m_zoomInButton);
  
  connect(m_zoomInButton, &QPushButton::clicked, this, &DebugPane::handleZoomIn);
  connect(m_zoomOutButton, &QPushButton::clicked, this, &DebugPane::handleZoomOut);
  
  // Create the actual devtools view
  m_devToolsView = new QWebEngineView(m_devToolsContainer);
  
  QWebEngineSettings *settings = m_devToolsView->page()->settings();
  settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
  settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
  
  // Set dark background while loading
  m_devToolsView->page()->setBackgroundColor(QColor("#1e1e1e"));
  
  devToolsLayout->addWidget(devToolsToolbar);
  devToolsLayout->addWidget(m_devToolsView);
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
  if (m_consoleContainer->parent())
  {
    m_consoleContainer->setParent(nullptr);
  }
  if (m_devToolsContainer->parent())
  {
    m_devToolsContainer->setParent(nullptr);
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
    fullViewLayout->addWidget(m_devToolsContainer);
    m_devToolsContainer->show();
    m_splitter->hide();
    break;
    
  case SideBySide:
    m_splitter->addWidget(m_consoleContainer);
    m_splitter->addWidget(m_devToolsContainer);
    m_splitter->setSizes({1000, 1000});
    fullViewLayout->addWidget(m_splitter);
    m_consoleContainer->show();
    m_devToolsContainer->show();
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
        /* Invert the entire DevTools UI for dark mode */
        :root {
          filter: invert(1) hue-rotate(180deg);
          background: #1e1e1e !important;
        }
        
        /* Re-invert images and icons to preserve their original colors */
        img, svg, video, canvas, embed, object,
        .cm-color-swatch, .color-swatch {
          filter: invert(1) hue-rotate(180deg);
        }
        
        /* Fix for syntax highlighting */
        .cm-s-default .cm-keyword { filter: invert(1) hue-rotate(180deg); }
        .cm-s-default .cm-string { filter: invert(1) hue-rotate(180deg); }
        .cm-s-default .cm-number { filter: invert(1) hue-rotate(180deg); }
      `;
      document.head.appendChild(style);
    })();
  )";
  
  m_devToolsView->page()->runJavaScript(darkModeCSS);
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

#include "moc_debugpane.cpp"

