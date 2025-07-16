#pragma once

#include <QWidget>
#include <QPropertyAnimation>

class LoadingOverlay : public QWidget
{
  Q_OBJECT

public:
  explicit LoadingOverlay(QWidget *parent = nullptr);
  ~LoadingOverlay();

  void fadeOut();
  void updateGeometry(const QRect &parentGeometry);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QPropertyAnimation *fadeAnimation;
};