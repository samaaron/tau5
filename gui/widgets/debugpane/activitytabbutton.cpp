#include "activitytabbutton.h"
#include "../../styles/StyleManager.h"
#include <QPainter>
#include <QPaintEvent>
#include <QEasingCurve>

ActivityTabButton::ActivityTabButton(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
    , m_hasUnread(false)
    , m_activityEnabled(true)
    , m_pulseOpacity(0.0)
{
    setCheckable(true);
    setFocusPolicy(Qt::NoFocus);
    
    m_baseStyle = QString(
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
        "  background: %5; "
        "} "
        "QPushButton:checked { "
        "  background: %6; "
        "  color: %7; "
        "}")
        .arg(StyleManager::Colors::primaryOrangeAlpha(180))
        .arg(StyleManager::Typography::MONOSPACE_FONT_FAMILY)
        .arg(StyleManager::Typography::FONT_SIZE_SMALL)
        .arg(StyleManager::Typography::FONT_WEIGHT_BOLD)
        .arg(StyleManager::Colors::primaryOrangeAlpha(25))
        .arg(StyleManager::Colors::primaryOrangeAlpha(51))
        .arg(StyleManager::Colors::PRIMARY_ORANGE);
    
    updateStyleSheet();
    
    m_pulseAnimation = std::make_unique<QPropertyAnimation>(this, "pulseOpacity");
    m_pulseAnimation->setDuration(1500);
    m_pulseAnimation->setStartValue(1.0);
    m_pulseAnimation->setEndValue(0.0);
    m_pulseAnimation->setEasingCurve(QEasingCurve::OutQuad);
}

ActivityTabButton::~ActivityTabButton() = default;

void ActivityTabButton::pulseActivity()
{
    if (!m_activityEnabled) {
        return;
    }
    
    if (m_pulseAnimation->state() == QPropertyAnimation::Running) {
        m_pulseAnimation->stop();
    }
    
    m_pulseOpacity = 1.0;
    m_pulseAnimation->start();
    update();
}

void ActivityTabButton::setHasUnread(bool hasUnread)
{
    if (!m_activityEnabled && hasUnread) {
        return;
    }
    
    if (m_hasUnread != hasUnread) {
        m_hasUnread = hasUnread;
        update();
    }
}

void ActivityTabButton::setPulseDuration(int ms)
{
    if (m_pulseAnimation) {
        m_pulseAnimation->setDuration(ms);
    }
}

void ActivityTabButton::setPulseOpacity(qreal opacity)
{
    if (m_pulseOpacity != opacity) {
        m_pulseOpacity = opacity;
        update();
    }
}

void ActivityTabButton::updateStyleSheet()
{
    setStyleSheet(m_baseStyle);
}

void ActivityTabButton::paintEvent(QPaintEvent *event)
{
    if (m_pulseOpacity > 0.0) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        QColor pulseColor = QColor(StyleManager::Colors::PRIMARY_ORANGE);
        pulseColor.setAlphaF(m_pulseOpacity * 0.6);
        painter.fillRect(rect(), pulseColor);
    }
    
    QPushButton::paintEvent(event);
    
    if (m_activityEnabled && m_hasUnread && !isChecked()) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        QColor underlineColor = QColor(StyleManager::Colors::PRIMARY_ORANGE);
        underlineColor.setAlphaF(0.5);
        
        QPen underlinePen(underlineColor);
        underlinePen.setWidth(2);
        painter.setPen(underlinePen);
        int y = height() - 2;
        painter.drawLine(4, y, width() - 4, y);
    }
}