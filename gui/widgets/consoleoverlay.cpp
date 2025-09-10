#include "consoleoverlay.h"
#include <QResizeEvent>
#include <QShowEvent>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QSizePolicy>
#include "../shared/tau5logger.h"
#include "../styles/StyleManager.h"

ConsoleOverlay::ConsoleOverlay(QWidget *parent)
    : QWidget(parent)
    , m_logWidget(new QTextEdit(this))
    , m_opacityEffect(new QGraphicsOpacityEffect(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setGraphicsEffect(m_opacityEffect);
    m_opacityEffect->setOpacity(1.0);
    
    m_logWidget->setReadOnly(true);
    m_logWidget->setFrameStyle(QFrame::NoFrame);
    m_logWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_logWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_logWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_logWidget);
    
    m_fadeAnimation = std::make_unique<QPropertyAnimation>(m_opacityEffect, "opacity", this);
    m_fadeAnimation->setDuration(500);
    m_fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    
    connect(m_fadeAnimation.get(), &QPropertyAnimation::finished, [this]() {
        if (m_opacityEffect->opacity() < 0.01) {
            hide();
            emit fadeComplete();
        }
    });
    
    setupStyles();
    // Don't position in constructor - wait for parent to be ready
    
    appendLog("[TAU5] System initializing...");
    appendLog("[BEAM] Starting Erlang VM...");
}

void ConsoleOverlay::setupStyles()
{
    setAttribute(Qt::WA_StyledBackground, false);
    
    m_logWidget->setStyleSheet(QString(R"(
        QTextEdit {
            background-color: transparent;
            color: %1;
            font-family: 'Cascadia Code PL', 'Cascadia Mono', 'Consolas', monospace;
            font-size: 10px;
            font-weight: 400;
            padding: 12px;
            border: none;
        }
        QTextEdit::selection {
            background-color: %2;
            color: %3;
        }
        %4
    )").arg(StyleManager::Colors::ACCENT_PRIMARY)
       .arg(StyleManager::Colors::accentPrimaryAlpha(0.4))
       .arg(StyleManager::Colors::TEXT_PRIMARY)
       .arg(StyleManager::contextMenu()));
}

void ConsoleOverlay::appendLog(const QString &message)
{
    // Split lines but preserve empty lines for proper formatting
    QStringList lines = message.split('\n', Qt::KeepEmptyParts);
    for (const QString &line : lines) {
        // Don't trim - preserve original formatting including leading spaces
        // Only skip completely empty messages (not lines)
        if (lines.size() == 1 && line.isEmpty()) {
            continue;
        }
        m_logLines.append(line);
        while (m_logLines.size() > MAX_LOG_LINES) {
            m_logLines.removeFirst();
        }
    }
    
    QString logText = m_logLines.join("\n");
    m_logWidget->setPlainText(logText);
    
    QScrollBar *scrollBar = m_logWidget->verticalScrollBar();
    if (scrollBar) {
        scrollBar->setValue(scrollBar->maximum());
    }
}

void ConsoleOverlay::clear()
{
    m_logLines.clear();
    m_logWidget->clear();
}

void ConsoleOverlay::fadeOut()
{
    if (m_fadeAnimation->state() != QAbstractAnimation::Running) {
        m_fadeAnimation->setStartValue(m_opacityEffect->opacity());
        m_fadeAnimation->setEndValue(0.0);
        m_fadeAnimation->start();
    }
}

void ConsoleOverlay::fadeIn()
{
    show();
    if (m_fadeAnimation->state() != QAbstractAnimation::Running) {
        m_fadeAnimation->setStartValue(m_opacityEffect->opacity());
        m_fadeAnimation->setEndValue(1.0);
        m_fadeAnimation->start();
    }
}

qreal ConsoleOverlay::opacity() const
{
    return m_opacityEffect->opacity();
}

void ConsoleOverlay::setOpacity(qreal opacity)
{
    m_opacityEffect->setOpacity(opacity);
}

void ConsoleOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionOverlay();
}

void ConsoleOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
}

void ConsoleOverlay::positionOverlay()
{
    if (parentWidget()) {
        // Keep fixed size - don't scale with parent
        int width = OVERLAY_WIDTH;
        int height = OVERLAY_HEIGHT;
        int x = parentWidget()->width() - width - MARGIN;
        int y = parentWidget()->height() - height - MARGIN;
        
        // Debug output
        Tau5Logger::instance().debug(QString("[ConsoleOverlay] Parent size: %1x%2, positioning at: %3,%4 size: %5x%6")
            .arg(parentWidget()->width())
            .arg(parentWidget()->height())
            .arg(x).arg(y)
            .arg(width).arg(height));
        
        setGeometry(x, y, width, height);
    }
}

void ConsoleOverlay::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QLinearGradient bgGradient(0, 0, 0, height());
    bgGradient.setColorAt(0, QColor(0, 0, 0, 80));
    bgGradient.setColorAt(1, QColor(0, 0, 0, 120));
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgGradient);
    painter.drawRoundedRect(rect(), 5, 5);
    
    // Use golden yellow (255, 215, 0) to match text color
    QRadialGradient glowGradient(rect().center(), rect().width() / 2);
    glowGradient.setColorAt(0, QColor(255, 215, 0, 60));
    glowGradient.setColorAt(0.5, QColor(255, 215, 0, 40));
    glowGradient.setColorAt(1, QColor(255, 215, 0, 0));
    
    QPen borderPen(QBrush(glowGradient), 3);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5), 5, 5);
    
    // Use golden yellow for inner border
    painter.setPen(QPen(QColor(255, 215, 0, 200), 2));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 5, 5);
    
    QWidget::paintEvent(event);
}