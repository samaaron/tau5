#include "customsplitter.h"
#include "../../styles/StyleManager.h"
#include <QPainter>

CustomSplitterHandle::CustomSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent), m_isHovered(false)
{
    setMouseTracking(true);
}

void CustomSplitterHandle::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    const int thickness = m_isHovered ? 6 : 1;
    const QColor color = m_isHovered ?
        QColor(StyleManager::Colors::PRIMARY_ORANGE) : QColor("#505050");

    if (orientation() == Qt::Horizontal)
    {
        int centerStart = (width() - thickness) / 2;
        painter.fillRect(centerStart, 0, thickness, height(), color);
    }
    else
    {
        int centerStart = (height() - thickness) / 2;
        painter.fillRect(0, centerStart, width(), thickness, color);
    }
}

void CustomSplitterHandle::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    m_isHovered = true;
    update();
}

void CustomSplitterHandle::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_isHovered = false;
    update();
}

CustomSplitter::CustomSplitter(Qt::Orientation orientation, QWidget *parent) 
    : QSplitter(orientation, parent) 
{
}

QSplitterHandle *CustomSplitter::createHandle()
{
    return new CustomSplitterHandle(orientation(), this);
}