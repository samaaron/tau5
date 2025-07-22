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
#include "../styles/StyleManager.h"

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
    if (!button) return nullptr;
    
    QTimer *timer = new QTimer(parent);
    timer->setInterval(200); // Update every 200ms for animation
    
    // Store frame count as timer property
    timer->setProperty("frame", 0);
    
    // Store original text
    QString originalText = button->text();
    timer->setProperty("originalText", originalText);
    
    QObject::connect(timer, &QTimer::timeout, [timer, button]() {
        int frame = timer->property("frame").toInt();
        frame = (frame + 1) % 8; // Cycle through 8 animation states
        timer->setProperty("frame", frame);
        
        // Simple text-based spinner animation using different Unicode characters
        // These create a rotating effect
        static const QChar spinnerChars[] = {
            QChar(0xEA6A), // sync icon (default)
            QChar(0xEB6E), // circle-outline
            QChar(0xEB6F), // circle-slash  
            QChar(0xEB6E), // circle-outline
            QChar(0xEA6A), // sync icon
            QChar(0xEB6E), // circle-outline
            QChar(0xEB6F), // circle-slash
            QChar(0xEB6E)  // circle-outline
        };
        
        button->setText(spinnerChars[frame]);
    });
    
    timer->start();
    return timer;
}

void AnimationControl::stopRestartAnimation(QTimer *&timer, QPushButton *button, const QIcon &originalIcon)
{
    if (timer) {
        // Restore original text if stored
        if (button) {
            QString originalText = timer->property("originalText").toString();
            if (!originalText.isEmpty()) {
                button->setText(originalText);
            }
        }
        
        timer->stop();
        timer->deleteLater();
        timer = nullptr;
    }
}