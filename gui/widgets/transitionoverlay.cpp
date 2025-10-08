#include "transitionoverlay.h"
#include "styles/StyleManager.h"
#include <QPainter>
#include <QResizeEvent>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

TransitionOverlay::TransitionOverlay(QWidget *parent)
    : QWidget(parent)
    , m_fadeAnimation(nullptr)
    , m_opacityEffect(nullptr)
{
    setupUi();
}

TransitionOverlay::~TransitionOverlay()
{
}

void TransitionOverlay::setupUi()
{
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);
    m_fadeAnimation = std::make_unique<QPropertyAnimation>(m_opacityEffect, "opacity");
    m_fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_fadeAnimation.get(), &QPropertyAnimation::finished, [this]() {
        qreal finalOpacity = m_opacityEffect->opacity();
        if (finalOpacity <= 0.01) {
            hide();
            emit fadeOutComplete();
        } else if (finalOpacity >= 0.99) {
            emit fadeInComplete();
        }
    });
}

void TransitionOverlay::fadeIn(int duration)
{
    show();
    // Don't raise - let MainWindow manage the z-order

    m_fadeAnimation->setDuration(duration);
    m_fadeAnimation->setStartValue(m_opacityEffect->opacity());
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->start();
}

void TransitionOverlay::fadeOut(int duration)
{
    m_fadeAnimation->setDuration(duration);
    m_fadeAnimation->setStartValue(m_opacityEffect->opacity());
    m_fadeAnimation->setEndValue(0.0);
    m_fadeAnimation->start();
}

void TransitionOverlay::setImmediateOpacity(qreal opacity)
{
    m_fadeAnimation->stop();
    m_opacityEffect->setOpacity(opacity);

    if (opacity <= 0.01) {
        hide();
    } else {
        show();
        raise();
    }
}

void TransitionOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void TransitionOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(0, 0, 0, 255));
}