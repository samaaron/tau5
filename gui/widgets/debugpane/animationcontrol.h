#ifndef DEBUGPANE_ANIMATIONCONTROL_H
#define DEBUGPANE_ANIMATIONCONTROL_H

#include <memory>

class QWidget;
class QPropertyAnimation;
class QTimer;
class QPushButton;
class QIcon;

class AnimationControl
{
public:
    static std::unique_ptr<QPropertyAnimation> createSlideAnimation(QWidget *target, const char *propertyName);
    static void performSlide(QPropertyAnimation *animation, bool show, int parentHeight, int paneHeight, int currentHeight);
    
    // Restart button animation
    static QTimer* createRestartAnimation(QWidget *parent, QPushButton *button, const QIcon &baseIcon);
    static void stopRestartAnimation(QTimer *&timer, QPushButton *button, const QIcon &originalIcon);
};

#endif // DEBUGPANE_ANIMATIONCONTROL_H