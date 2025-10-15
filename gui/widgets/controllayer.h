#ifndef CONTROLLAYER_H
#define CONTROLLAYER_H

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>

// Custom button class with circular background and SVG cutout
class CircularButton : public QPushButton
{
    Q_OBJECT
public:
    explicit CircularButton(const QString &text, QWidget *parent = nullptr)
        : QPushButton(text, parent), m_hovered(false), m_hoverProgress(0.0)
    {
        setMouseTracking(true);

        // Setup animation timer
        m_animationTimer = new QTimer(this);
        m_animationTimer->setInterval(16); // ~60 FPS
        connect(m_animationTimer, &QTimer::timeout, this, &CircularButton::updateAnimation);
    }

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void updateAnimation();

private:
    bool m_hovered;
    qreal m_hoverProgress;  // 0.0 (not hovered) to 1.0 (fully hovered)
    QTimer *m_animationTimer;
};

class ControlLayer : public QWidget
{
    Q_OBJECT

public:
    explicit ControlLayer(QWidget *parent = nullptr);
    
    void setConsoleVisible(bool visible);
    void setDebugPaneAvailable(bool available);
    void positionControls();

signals:
    void sizeDown();
    void sizeUp();
    void openExternalBrowser();
    void resetBrowser();
    void toggleConsole();
    void saveAsImage();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupControls();
    void connectSignals();
    
    CircularButton *m_sizeDownButton;
    CircularButton *m_sizeUpButton;
    CircularButton *m_openExternalBrowserButton;
    CircularButton *m_resetBrowserButton;
    CircularButton *m_consoleToggleButton;
    CircularButton *m_saveImageButton;
    
    QHBoxLayout *m_buttonLayout;
    
    bool m_consoleVisible;
};

#endif // CONTROLLAYER_H