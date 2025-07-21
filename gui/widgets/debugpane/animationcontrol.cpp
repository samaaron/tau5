#include "animationcontrol.h"
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QWidget>
#include <QTimer>
#include <QPushButton>
#include <QPixmap>
#include <QTransform>
#include <QPainter>
#include <QIcon>

std::unique_ptr<QPropertyAnimation> AnimationControl::createSlideAnimation(QWidget *target, const char *propertyName)
{
    auto animation = std::make_unique<QPropertyAnimation>(target, propertyName);
    animation->setDuration(300);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    return animation;
}

void AnimationControl::performSlide(QPropertyAnimation *animation, bool show, int parentHeight, int paneHeight, int currentHeight)
{
    if (!animation) return;
    
    if (show) {
        // Slide up from bottom
        animation->setStartValue(parentHeight);
        animation->setEndValue(parentHeight - paneHeight);
    } else {
        // Slide down to hide
        animation->setStartValue(parentHeight - currentHeight);
        animation->setEndValue(parentHeight);
    }
    
    animation->start();
}

QTimer* AnimationControl::createRestartAnimation(QWidget *parent, QPushButton *button, const QIcon &baseIcon)
{
    if (!button || baseIcon.isNull()) return nullptr;
    
    QTimer *timer = new QTimer(parent);
    timer->setInterval(50); // Update every 50ms for smooth rotation
    
    // Store frame count as timer property
    timer->setProperty("frame", 0);
    QPixmap basePixmap = baseIcon.pixmap(24, 24);
    
    QObject::connect(timer, &QTimer::timeout, [timer, button, basePixmap]() {
        int frame = timer->property("frame").toInt();
        frame = (frame + 1) % 36;
        timer->setProperty("frame", frame);
        
        QTransform transform;
        transform.rotate(-frame * 10);
        QPixmap rotatedPixmap = basePixmap.transformed(transform, Qt::SmoothTransformation);
        button->setIcon(QIcon(rotatedPixmap));
        button->setIconSize(QSize(24, 24));
    });
    
    timer->start();
    return timer;
}

void AnimationControl::stopRestartAnimation(QTimer *&timer, QPushButton *button, const QIcon &originalIcon)
{
    if (timer) {
        timer->stop();
        timer->deleteLater();
        timer = nullptr;
    }
    
    if (button && !originalIcon.isNull()) {
        button->setIcon(originalIcon);
    }
}