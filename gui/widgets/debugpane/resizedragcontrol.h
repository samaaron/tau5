#ifndef DEBUGPANE_RESIZEDRAGCONTROL_H
#define DEBUGPANE_RESIZEDRAGCONTROL_H

#include <QWidget>
#include <functional>

class QMouseEvent;

class ResizeDragControl
{
public:
    struct ResizeState {
        bool isResizing;
        int resizeStartY;
        int resizeStartHeight;
        bool isHoveringHandle;
    };
    
    static QWidget* createDragHandle(QWidget *parent, int height, const QString &color);
    
    static void startResize(ResizeState &state, QMouseEvent *event, int currentHeight, QWidget *dragHandle);
    
    static void handleMouseMove(ResizeState &state, QMouseEvent *event, QWidget *widget, 
                               QWidget *dragHandle, int headerHeight, int handleHeight,
                               std::function<int(int)> constrainHeight);
    
    static void finishResize(ResizeState &state, QWidget *dragHandle);
    
    static void updateDragHandlePosition(QWidget *dragHandle, QWidget *parent, int handleHeight);
    
    static bool isInResizeArea(const QPoint &pos, QWidget *widget, QWidget *headerWidget, 
                              int handleHeight);
};

#endif // DEBUGPANE_RESIZEDRAGCONTROL_H