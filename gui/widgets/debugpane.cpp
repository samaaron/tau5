#include "debugpane.h"
#include "StyleManager.h"
#include "phxwebview.h"
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QScrollBar>
#include <QStackedWidget>
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

DebugPane::DebugPane(QWidget *parent)
    : QWidget(parent), m_isVisible(false), m_autoScroll(true), m_maxLines(5000),
      m_currentMode(BeamLogOnly), m_isResizing(false), m_resizeStartY(0),
      m_resizeStartHeight(0), m_targetWebView(nullptr), m_devToolsView(nullptr)
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

  m_contentStack = new QStackedWidget(this);
  m_splitter = new QSplitter(Qt::Horizontal, this);

  QWidget *consoleClone = new QWidget();
  QVBoxLayout *consoleCloneLayout = new QVBoxLayout(consoleClone);
  consoleCloneLayout->setContentsMargins(0, 0, 0, 0);
  consoleCloneLayout->setSpacing(0);

  QTextEdit *clonedOutput = new QTextEdit(consoleClone);
  clonedOutput->setReadOnly(true);
  clonedOutput->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  clonedOutput->setStyleSheet(StyleManager::consoleOutput());
  consoleCloneLayout->addWidget(clonedOutput);

  connect(m_outputDisplay, &QTextEdit::textChanged, this, [clonedOutput, this]()
          {
        clonedOutput->setHtml(m_outputDisplay->toHtml());
        if (m_autoScroll) {
            QScrollBar *scrollBar = clonedOutput->verticalScrollBar();
            scrollBar->setValue(scrollBar->maximum());
        } });

  m_splitter->addWidget(consoleClone);
  m_splitter->addWidget(m_devToolsView);
  m_splitter->setSizes({1000, 1000});

  m_contentStack->addWidget(m_consoleWidget);
  m_contentStack->addWidget(new QWidget());
  m_contentStack->addWidget(m_splitter);

  m_mainLayout->addWidget(m_headerWidget);
  m_mainLayout->addWidget(m_contentStack, 1);

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
      terminalSvg.arg(hoverColor),
      terminalSvg.arg(selectedColor));

  QString bugSvg = QString("<svg viewBox='0 0 16 16' fill='%1'><path fill-rule='evenodd' clip-rule='evenodd' d='M10.877 4.5v-.582a2.918 2.918 0 1 0-5.836 0V4.5h-.833L2.545 2.829l-.593.59 1.611 1.619-.019.049a8.03 8.03 0 0 0-.503 2.831c0 .196.007.39.02.58l.003.045H1v.836h2.169l.006.034c.172.941.504 1.802.954 2.531l.034.055L2.2 13.962l.592.592 1.871-1.872.058.066c.868.992 2.002 1.589 3.238 1.589 1.218 0 2.336-.579 3.199-1.544l.057-.064 1.91 1.92.593-.591-1.996-2.006.035-.056c.467-.74.81-1.619.986-2.583l.006-.034h2.171v-.836h-2.065l.003-.044a8.43 8.43 0 0 0 .02-.58 8.02 8.02 0 0 0-.517-2.866l-.019-.05 1.57-1.57-.592-.59L11.662 4.5h-.785zm-5 0v-.582a2.082 2.082 0 1 1 4.164 0V4.5H5.878z'/></svg>");

  QIcon devToolsIcon = createSvgIcon(
      bugSvg.arg(normalColor),
      bugSvg.arg(hoverColor),
      bugSvg.arg(selectedColor));

  QString splitSvg = QString("<svg viewBox='0 0 16 16' fill='%1'><path d='M14 1H3L2 2v11l1 1h11l1-1V2l-1-1zM8 13H3V2h5v11zm6 0H9V2h5v11z'/></svg>");
  QIcon sideBySideIcon = createSvgIcon(
      splitSvg.arg(normalColor),
      splitSvg.arg(hoverColor),
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
      "QPushButton:hover { } "
      "QPushButton:pressed { }"

  );

  m_beamLogButton->setStyleSheet(buttonStyle);
  m_devToolsButton->setStyleSheet(buttonStyle);
  m_sideBySideButton->setStyleSheet(buttonStyle);

  m_beamLogButton->setCheckable(true);
  m_devToolsButton->setCheckable(true);
  m_sideBySideButton->setCheckable(true);

  QLabel *scrollLabel = new QLabel("Auto-scroll", m_headerWidget);
  scrollLabel->setStyleSheet(
      QString("QLabel { "
              "  color: %1; "
              "  font-family: %2; "
              "  font-size: %3; "
              "  font-weight: %4; "
              "  background: transparent; "
              "  margin-right: %5; "
              "}")
          .arg(StyleManager::Colors::PRIMARY_ORANGE)
          .arg(StyleManager::Typography::MONOSPACE_FONT_FAMILY)
          .arg(StyleManager::Typography::FONT_SIZE_SMALL)
          .arg(StyleManager::Typography::FONT_WEIGHT_BOLD)
          .arg(StyleManager::Spacing::SMALL));

  m_autoScrollToggle = new QCheckBox(m_headerWidget);
  m_autoScrollToggle->setToolTip("Toggle Auto-scroll");
  m_autoScrollToggle->setChecked(true);
  m_autoScrollToggle->setStyleSheet(StyleManager::checkbox());

  m_headerLayout->addWidget(titleLabel);
  m_headerLayout->addStretch();
  m_headerLayout->addWidget(scrollLabel);
  m_headerLayout->addWidget(m_autoScrollToggle);

  QLabel *separator = new QLabel("|", m_headerWidget);
  separator->setStyleSheet(
      QString("QLabel { "
              "  color: %1; "
              "  margin: 0 8px; "
              "  background: transparent; "
              "}")
          .arg(StyleManager::Colors::primaryOrangeAlpha(100)));
  m_headerLayout->addWidget(separator);

  m_headerLayout->addWidget(m_beamLogButton);
  m_headerLayout->addWidget(m_devToolsButton);
  m_headerLayout->addWidget(m_sideBySideButton);

  connect(m_beamLogButton, &QPushButton::clicked, this, &DebugPane::showBeamLogOnly);
  connect(m_devToolsButton, &QPushButton::clicked, this, &DebugPane::showDevToolsOnly);
  connect(m_sideBySideButton, &QPushButton::clicked, this, &DebugPane::showSideBySide);
  connect(m_autoScrollToggle, &QCheckBox::toggled, this, &DebugPane::handleAutoScrollToggled);
}

void DebugPane::setupConsole()
{
  m_consoleWidget = new QWidget();
  m_consoleLayout = new QVBoxLayout(m_consoleWidget);
  m_consoleLayout->setContentsMargins(0, 0, 0, 0);
  m_consoleLayout->setSpacing(0);

  m_outputDisplay = new QTextEdit(m_consoleWidget);
  m_outputDisplay->setReadOnly(true);
  m_outputDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_outputDisplay->setStyleSheet(StyleManager::consoleOutput());

  m_consoleLayout->addWidget(m_outputDisplay);
}

void DebugPane::setupDevTools()
{
  m_devToolsView = new QWebEngineView(this);
  
  QWebEngineSettings *settings = m_devToolsView->page()->settings();
  settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
  settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
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

  switch (m_currentMode)
  {
  case BeamLogOnly:
    m_contentStack->setCurrentIndex(0);
    break;
  case DevToolsOnly:
  {
    m_contentStack->setCurrentIndex(2);
    QWidget *consoleClone = m_splitter->widget(0);
    if (consoleClone)
    {
      consoleClone->hide();
    }
    if (m_devToolsView)
    {
      m_devToolsView->show();
    }
  }
  break;
  case SideBySide:
  {
    m_contentStack->setCurrentIndex(2);
    QWidget *consoleCloneForBoth = m_splitter->widget(0);
    if (consoleCloneForBoth)
    {
      consoleCloneForBoth->show();
    }
    if (m_devToolsView)
    {
      m_devToolsView->show();
    }
  }
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

  if (!hoverSvg.isEmpty())
  {
    QByteArray hoverBytes = hoverSvg.toUtf8();
    QPixmap hoverPixmap(32, 32);
    hoverPixmap.fill(Qt::transparent);
    QSvgRenderer hoverRenderer(hoverBytes);
    if (hoverRenderer.isValid())
    {
      QPainter painter(&hoverPixmap);
      painter.setRenderHint(QPainter::Antialiasing);
      hoverRenderer.render(&painter);
    }
    icon.addPixmap(hoverPixmap, QIcon::Active, QIcon::Off);
  }

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
    icon.addPixmap(selectedPixmap, QIcon::Active, QIcon::On);
    icon.addPixmap(selectedPixmap, QIcon::Selected, QIcon::On);
  }

  return icon;
}

#include "moc_debugpane.cpp"

