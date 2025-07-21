#include "customsplitter.h"
#include "../debugpane.h"
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