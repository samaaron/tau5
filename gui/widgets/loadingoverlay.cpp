#include "loadingoverlay.h"
#include <QPainter>
#include <QTimer>

LoadingOverlay::LoadingOverlay(QWidget *parent)
    : QWidget(nullptr)
    , fadeAnimation(nullptr)
{
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  
  fadeAnimation = new QPropertyAnimation(this, "windowOpacity", this);
  fadeAnimation->setDuration(500);
  fadeAnimation->setStartValue(1.0);
  fadeAnimation->setEndValue(0.0);
  
  connect(fadeAnimation, &QPropertyAnimation::finished, this, &QWidget::close);
  
  setWindowOpacity(1.0);
}

LoadingOverlay::~LoadingOverlay() = default;

void LoadingOverlay::fadeOut()
{
  if (fadeAnimation && fadeAnimation->state() != QAbstractAnimation::Running) {
    fadeAnimation->start();
  }
}

void LoadingOverlay::updateGeometry(const QRect &parentGeometry)
{
  setGeometry(parentGeometry);
}

void LoadingOverlay::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
  
  QPainter painter(this);
  painter.fillRect(rect(), QColor(0, 0, 0, 255));
}