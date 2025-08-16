#ifndef TRANSITIONOVERLAY_H
#define TRANSITIONOVERLAY_H

#include <QWidget>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <memory>

class QLabel;

class TransitionOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit TransitionOverlay(QWidget *parent = nullptr);
    ~TransitionOverlay();

    void fadeIn(int duration = 300);
    void fadeOut(int duration = 300);
    void setImmediateOpacity(qreal opacity);

signals:
    void fadeInComplete();
    void fadeOutComplete();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi();
    
    std::unique_ptr<QPropertyAnimation> m_fadeAnimation;
    QGraphicsOpacityEffect *m_opacityEffect;
};

#endif // TRANSITIONOVERLAY_H