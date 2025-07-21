#include "resizedragcontrol.h"
#include <QMouseEvent>
#include <QCursor>

QWidget* ResizeDragControl::createDragHandle(QWidget *parent, int height, const QString &color)
{
    QWidget *handle = new QWidget(parent);
    handle->setFixedHeight(height);
    handle->setMouseTracking(true);
    handle->hide();
    handle->setStyleSheet(QString("background-color: %1;").arg(color));
    return handle;
}

void ResizeDragControl::startResize(ResizeState &state, QMouseEvent *event, int currentHeight, QWidget *dragHandle)
{
    state.isResizing = true;
    state.resizeStartY = event->globalPosition().y();
    state.resizeStartHeight = currentHeight;
    if (dragHandle) {
        dragHandle->show();
    }
}

void ResizeDragControl::handleMouseMove(ResizeState &state, QMouseEvent *event, QWidget *widget, 
                                       QWidget *dragHandle, int headerHeight, int handleHeight,
                                       std::function<int(int)> constrainHeight)
{
    if (state.isResizing) {
        int deltaY = state.resizeStartY - event->globalPosition().y();
        int newHeight = state.resizeStartHeight + deltaY;
        if (constrainHeight) {
            newHeight = constrainHeight(newHeight);
        }
        
        widget->resize(widget->width(), newHeight);
        widget->move(widget->x(), widget->parentWidget()->height() - newHeight);
    } else {
        // Check if hovering over resize area
        bool inResizeArea = isInResizeArea(event->position().toPoint(), widget, nullptr, handleHeight);
        
        if (inResizeArea) {
            widget->setCursor(Qt::SizeVerCursor);
            if (!state.isHoveringHandle) {
                state.isHoveringHandle = true;
                if (dragHandle) {
                    dragHandle->show();
                    dragHandle->raise();
                }
            }
        } else {
            widget->setCursor(Qt::ArrowCursor);
            if (state.isHoveringHandle) {
                state.isHoveringHandle = false;
                if (dragHandle) {
                    dragHandle->hide();
                }
            }
        }
    }
}

void ResizeDragControl::finishResize(ResizeState &state, QWidget *dragHandle)
{
    if (state.isResizing) {
        state.isResizing = false;
        if (!state.isHoveringHandle && dragHandle) {
            dragHandle->hide();
        }
    }
}

void ResizeDragControl::updateDragHandlePosition(QWidget *dragHandle, QWidget *parent, int handleHeight)
{
    if (dragHandle && parent) {
        dragHandle->resize(parent->width(), handleHeight);
        dragHandle->move(0, 0);
    }
}

bool ResizeDragControl::isInResizeArea(const QPoint &pos, QWidget *widget, QWidget *headerWidget, 
                                      int handleHeight)
{
    if (!widget) return false;
    
    // Check if near top edge of widget
    return pos.y() >= 0 && pos.y() <= handleHeight;
}