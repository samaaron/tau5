#include "consolewidget.h"
#include "StyleManager.h"
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QScrollBar>
#include <QDateTime>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QGraphicsBlurEffect>
#include <QTimer>
#include <QEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QCursor>
#include <QClipboard>
#include <QGuiApplication>
#include <QMainWindow>

ConsoleWidget::ConsoleWidget(QWidget *parent)
    : QWidget(parent), m_isVisible(false), m_autoScroll(true), m_maxLines(5000), m_isResizing(false), m_resizeStartY(0), m_resizeStartHeight(0)
{
  setAttribute(Qt::WA_TranslucentBackground);
  setWindowFlags(Qt::FramelessWindowHint);
  setMouseTracking(true);
  setMinimumHeight(100);
  setupUi();
  hide();
}

ConsoleWidget::~ConsoleWidget() = default;

void ConsoleWidget::setupUi()
{
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  m_mainLayout->setSpacing(0);

  QWidget *header = new QWidget(this);
  header->setStyleSheet(StyleManager::consoleHeader());
  header->setMouseTracking(true);

  m_buttonLayout = new QHBoxLayout(header);
  m_buttonLayout->setContentsMargins(10, 5, 10, 5);

  QLabel *titleLabel = new QLabel("BEAM Log", header);
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

  QLabel *scrollLabel = new QLabel("Auto-scroll", header);
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

  m_autoScrollToggle = new QCheckBox(header);
  m_autoScrollToggle->setToolTip("Toggle Auto-scroll");
  m_autoScrollToggle->setChecked(true);
  m_autoScrollToggle->setStyleSheet(StyleManager::checkbox());

  m_buttonLayout->addWidget(titleLabel);
  m_buttonLayout->addStretch();
  m_buttonLayout->addWidget(scrollLabel);
  m_buttonLayout->addWidget(m_autoScrollToggle);

  m_outputDisplay = new QTextEdit(this);
  m_outputDisplay->setReadOnly(true);
  m_outputDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_outputDisplay->setStyleSheet(StyleManager::consoleOutput());

  m_mainLayout->addWidget(header);
  m_mainLayout->addWidget(m_outputDisplay, 1);

  setStyleSheet(
      QString("ConsoleWidget { "
              "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
              "    stop:0 %1, "
              "    stop:0.1 %2, "
              "    stop:0.2 %1, "
              "    stop:0.8 %1, "
              "    stop:0.9 %2, "
              "    stop:1 %1); "
              "  border-top: 2px solid %3; "
              "  border-bottom: 1px solid %4; "
              "}")
          .arg(StyleManager::Colors::blackAlpha(191))
          .arg(StyleManager::Colors::primaryOrangeAlpha(64))
          .arg(StyleManager::Colors::primaryOrangeAlpha(150))
          .arg(StyleManager::Colors::primaryOrangeAlpha(100)));

  connect(m_autoScrollToggle, &QCheckBox::toggled, this, &ConsoleWidget::handleAutoScrollToggled);

  m_slideAnimation = std::make_unique<QPropertyAnimation>(this, "slidePosition");
  m_slideAnimation->setDuration(300);
  m_slideAnimation->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_slideAnimation.get(), &QPropertyAnimation::finished,
          this, &ConsoleWidget::animationFinished);

  // Simple approach: just use Qt's built-in scrollbar with proper styling
}

void ConsoleWidget::appendOutput(const QString &text, bool isError)
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

void ConsoleWidget::toggle()
{
  slide(!m_isVisible);
}

void ConsoleWidget::slide(bool show)
{
  if (show == m_isVisible)
    return;

  if (!parentWidget())
    return;

  int parentHeight = parentWidget()->height();
  int parentWidth = parentWidget()->width();
  int consoleHeight = height();

  // If height is 0 (first time showing), use default height
  if (consoleHeight <= 0)
  {
    consoleHeight = parentHeight / 3; // Default to 1/3 of parent height
  }

  // Always ensure console width matches parent width
  resize(parentWidth, consoleHeight);

  if (show)
  {
    // Start position: completely hidden below parent
    move(0, parentHeight);
    QWidget::show();

    // Z-ordering will be handled by MainWindow - just ensure we're visible
    raise();

    // Animate to visible position (bottom of parent - full stretch)
    m_slideAnimation->setStartValue(parentHeight);
    m_slideAnimation->setEndValue(parentHeight - consoleHeight);
  }
  else
  {
    // Animate to hidden position
    m_slideAnimation->setStartValue(parentHeight - height());
    m_slideAnimation->setEndValue(parentHeight);
  }

  m_slideAnimation->start();
  m_isVisible = show;
}

void ConsoleWidget::handleAutoScrollToggled(bool checked)
{
  m_autoScroll = checked;
}

void ConsoleWidget::animationFinished()
{
  if (!m_isVisible)
  {
    hide();
  }
  else
  {
    // Keep console raised when animation finishes
    raise();
    // Signal visibility change so MainWindow can ensure GUI buttons stay on top
  }
  emit visibilityChanged(m_isVisible);
}

bool ConsoleWidget::eventFilter(QObject *obj, QEvent *event)
{
  Q_UNUSED(obj);
  Q_UNUSED(event);
  return QWidget::eventFilter(obj, event);
}

void ConsoleWidget::mousePressEvent(QMouseEvent *event)
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

void ConsoleWidget::mouseMoveEvent(QMouseEvent *event)
{
  if (m_isResizing)
  {
    int deltaY = m_resizeStartY - event->globalPosition().y();
    int newHeight = m_resizeStartHeight + deltaY;

    // Limit height between reasonable bounds
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

void ConsoleWidget::mouseReleaseEvent(QMouseEvent *event)
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

void ConsoleWidget::enterEvent(QEnterEvent *event)
{
  QWidget::enterEvent(event);
}

void ConsoleWidget::leaveEvent(QEvent *event)
{
  if (!m_isResizing)
  {
    setCursor(Qt::ArrowCursor);
  }
  QWidget::leaveEvent(event);
}
