#ifndef ACTIVITYTABBUTTON_H
#define ACTIVITYTABBUTTON_H

#include <QPushButton>
#include <QPropertyAnimation>
#include <QTimer>
#include <memory>

class ActivityTabButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(qreal pulseOpacity READ pulseOpacity WRITE setPulseOpacity)
    
public:
    explicit ActivityTabButton(const QString &text, QWidget *parent = nullptr);
    ~ActivityTabButton();
    
    void pulseActivity();
    void setHasUnread(bool hasUnread);
    bool hasUnread() const { return m_hasUnread; }
    
    // Configuration
    void setActivityIndicatorsEnabled(bool enabled) { m_activityEnabled = enabled; }
    bool activityIndicatorsEnabled() const { return m_activityEnabled; }
    void setPulseDuration(int ms);
    
    qreal pulseOpacity() const { return m_pulseOpacity; }
    void setPulseOpacity(qreal opacity);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    
private:
    void startPulseAnimation();
    void updateStyleSheet();
    
private:
    bool m_hasUnread;
    bool m_activityEnabled = true;
    qreal m_pulseOpacity;
    std::unique_ptr<QPropertyAnimation> m_pulseAnimation;
    std::unique_ptr<QTimer> m_pulseTimer;
    QString m_baseStyle;
};

#endif // ACTIVITYTABBUTTON_H