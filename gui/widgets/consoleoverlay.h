#ifndef CONSOLEOVERLAY_H
#define CONSOLEOVERLAY_H

#include <QWidget>
#include <QTextEdit>
#include <QStringList>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <memory>

class ConsoleOverlay : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    explicit ConsoleOverlay(QWidget *parent = nullptr);
    ~ConsoleOverlay() = default;

    void appendLog(const QString &message);
    void fadeOut();
    void fadeIn();
    void clear();
    
    qreal opacity() const;
    void setOpacity(qreal opacity);

signals:
    void fadeComplete();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void positionOverlay();
    void setupStyles();
    
    QTextEdit *m_logWidget;
    QStringList m_logLines;
    std::unique_ptr<QPropertyAnimation> m_fadeAnimation;
    QGraphicsOpacityEffect *m_opacityEffect;
    
    static constexpr int MAX_LOG_LINES = 100;
    static constexpr int OVERLAY_WIDTH = 500;
    static constexpr int OVERLAY_HEIGHT = 300;
    static constexpr int MARGIN = 20;
};

#endif // CONSOLEOVERLAY_H