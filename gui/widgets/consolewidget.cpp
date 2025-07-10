#include "consolewidget.h"
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
    : QWidget(parent)
    , m_isVisible(false)
    , m_autoScroll(true)
    , m_maxLines(5000)
    , m_isResizing(false)
    , m_resizeStartY(0)
    , m_resizeStartHeight(0)
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
    header->setStyleSheet(
        "QWidget { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "    stop:0 rgba(26, 26, 26, 191), "
        "    stop:0.5 rgba(15, 15, 15, 191), "
        "    stop:1 rgba(0, 0, 0, 191)); "
        "  padding-top: 6px; "
        "}"
    );
    header->setMouseTracking(true);

    m_buttonLayout = new QHBoxLayout(header);
    m_buttonLayout->setContentsMargins(10, 5, 10, 5);

    QLabel *titleLabel = new QLabel("BEAM Log", header);
    titleLabel->setStyleSheet(
        "QLabel { "
        "  color: #ffa500; "
        "  background: transparent; "
        "  font-family: 'Consolas', 'Courier New', monospace; "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "  letter-spacing: 1px; "
        "  padding: 4px 12px; "
        "}"
    );

    QLabel *scrollLabel = new QLabel("Auto-scroll", header);
    scrollLabel->setStyleSheet(
        "QLabel { "
        "  color: #ffa500; "
        "  font-family: 'Consolas', monospace; "
        "  font-size: 10px; "
        "  font-weight: bold; "
        "  background: transparent; "
        "  margin-right: 5px; "
        "}"
    );

    m_autoScrollToggle = new QCheckBox(header);
    m_autoScrollToggle->setToolTip("Toggle Auto-scroll");
    m_autoScrollToggle->setChecked(true);
    m_autoScrollToggle->setStyleSheet(
        "QCheckBox { "
        "  background: transparent; "
        "  color: #ffa500; "
        "  font-family: 'Consolas', monospace; "
        "  font-size: 10px; "
        "  font-weight: bold; "
        "  spacing: 5px; "
        "}"
        "QCheckBox::indicator { "
        "  width: 16px; "
        "  height: 16px; "
        "  border-radius: 3px; "
        "  background: rgba(0, 0, 0, 150); "
        "  border: 2px solid rgba(255, 165, 0, 150); "
        "}"
        "QCheckBox::indicator:checked { "
        "  background: rgba(255, 165, 0, 200); "
        "  border: 2px solid rgba(255, 165, 0, 255); "
        "}"
        "QCheckBox::indicator:checked::after { "
        "  content: '✓'; "
        "  color: white; "
        "  font-weight: bold; "
        "  font-size: 12px; "
        "}"
        "QCheckBox::indicator:hover { "
        "  border: 2px solid rgba(255, 165, 0, 255); "
        "}"
    );

    m_buttonLayout->addWidget(titleLabel);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(scrollLabel);
    m_buttonLayout->addWidget(m_autoScrollToggle);

    m_outputDisplay = new QTextEdit(this);
    m_outputDisplay->setReadOnly(true);
    m_outputDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_outputDisplay->setStyleSheet(
        "QTextEdit { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "    stop:0 rgba(5, 5, 5, 191), "
        "    stop:0.3 rgba(0, 0, 0, 191), "
        "    stop:0.7 rgba(0, 0, 0, 191), "
        "    stop:1 rgba(10, 10, 10, 191)); "
        "  color: #ffa500; "
        "  font-family: 'Consolas', 'Monaco', 'Courier New', monospace; "
        "  font-size: 12px; "
        "  border: none; "
        "  padding: 12px; "
        "  selection-background-color: rgba(255, 165, 0, 100); "
        "  selection-color: #ffffff; "
        "}"
        "QScrollBar:vertical { "
        "  background: rgba(0, 0, 0, 0); "
        "  width: 12px; "
        "  border: none; "
        "  margin: 0px; "
        "}"
        "QScrollBar::handle:vertical { "
        "  background: rgba(255, 165, 0, 180); "
        "  border-radius: 6px; "
        "  min-height: 20px; "
        "  margin: 2px; "
        "  border: none; "
        "}"
        "QScrollBar::handle:vertical:hover { "
        "  background: rgba(255, 165, 0, 255); "
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "  height: 0px; "
        "  background: rgba(0, 0, 0, 0); "
        "  border: none; "
        "}"
        "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical { "
        "  background: rgba(0, 0, 0, 0); "
        "  border: none; "
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
        "  background: rgba(0, 0, 0, 0); "
        "  border: none; "
        "}"
    );

    m_mainLayout->addWidget(header);
    m_mainLayout->addWidget(m_outputDisplay, 1);

    setStyleSheet(
        "ConsoleWidget { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "    stop:0 rgba(0, 0, 0, 191), "
        "    stop:0.1 rgba(255, 165, 0, 64), "
        "    stop:0.2 rgba(0, 0, 0, 191), "
        "    stop:0.8 rgba(0, 0, 0, 191), "
        "    stop:0.9 rgba(255, 165, 0, 64), "
        "    stop:1 rgba(0, 0, 0, 191)); "
        "  border-top: 2px solid rgba(255, 165, 0, 150); "
        "  border-bottom: 1px solid rgba(255, 165, 0, 100); "
        "}"
    );

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
    if (text.isEmpty()) return;

    QTextCursor cursor = m_outputDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);

    QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss.zzz] ");

    QTextCharFormat format;
    format.setForeground(isError ? QColor("#4169e1") : QColor("#ffa500"));

    QTextCharFormat timestampFormat;
    timestampFormat.setForeground(QColor("#888888"));
    cursor.setCharFormat(timestampFormat);
    cursor.insertText(timestamp);

    cursor.setCharFormat(format);
    cursor.insertText(text);

    if (!text.endsWith('\n')) {
        cursor.insertText("\n");
    }

    if (m_outputDisplay->document()->lineCount() > m_maxLines) {
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor,
                          m_outputDisplay->document()->lineCount() - m_maxLines);
        cursor.removeSelectedText();
    }

    if (m_autoScroll) {
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
    if (show == m_isVisible) return;

    if (!parentWidget()) return;

    int parentHeight = parentWidget()->height();
    int parentWidth = parentWidget()->width();
    int consoleHeight = height();

    // If height is 0 (first time showing), use default height
    if (consoleHeight <= 0) {
        consoleHeight = parentHeight / 3; // Default to 1/3 of parent height
    }

    // Always ensure console width matches parent width
    resize(parentWidth, consoleHeight);

    if (show) {
        // Start position: completely hidden below parent
        move(0, parentHeight);
        QWidget::show();

        // Z-ordering will be handled by MainWindow - just ensure we're visible
        raise();

        // Animate to visible position (bottom of parent - full stretch)
        m_slideAnimation->setStartValue(parentHeight);
        m_slideAnimation->setEndValue(parentHeight - consoleHeight);
    } else {
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
    if (!m_isVisible) {
        hide();
    } else {
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
        event->position().y() < RESIZE_HANDLE_HEIGHT) {
        m_isResizing = true;
        m_resizeStartY = event->globalPosition().y();
        m_resizeStartHeight = height();
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void ConsoleWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isResizing) {
        int deltaY = m_resizeStartY - event->globalPosition().y();
        int newHeight = m_resizeStartHeight + deltaY;

        // Limit height between reasonable bounds
        int minHeight = 100;
        int maxHeight = parentWidget() ? parentWidget()->height() * 0.8 : 600;
        newHeight = qBound(minHeight, newHeight, maxHeight);

        resize(width(), newHeight);
        move(x(), parentWidget()->height() - newHeight);
        event->accept();
    } else if (event->position().y() < RESIZE_HANDLE_HEIGHT) {
        setCursor(Qt::SizeVerCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }

    QWidget::mouseMoveEvent(event);
}

void ConsoleWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isResizing) {
        m_isResizing = false;
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void ConsoleWidget::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
}

void ConsoleWidget::leaveEvent(QEvent *event)
{
    if (!m_isResizing) {
        setCursor(Qt::ArrowCursor);
    }
    QWidget::leaveEvent(event);
}

