#ifndef CUSTOMTITLEBAR_H
#define CUSTOMTITLEBAR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

class CustomTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit CustomTitleBar(QWidget *parent = nullptr);
    ~CustomTitleBar();

    void setTitle(const QString &title);
    QString title() const;
    
    // Getters for system buttons (needed by QWindowKit)
    QPushButton* minimizeButton() const { return m_minimizeButton; }
    QPushButton* maximizeButton() const { return m_maximizeButton; }
    QPushButton* closeButton() const { return m_closeButton; }

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

public slots:
    void updateMaximizeButton();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void setupUi();
    void applyStyles();
    
    QLabel *m_titleLabel;
    QPushButton *m_minimizeButton;
    QPushButton *m_maximizeButton;
    QPushButton *m_closeButton;
    QHBoxLayout *m_layout;
    
    // Platform-specific button sizing - made smaller
#ifdef Q_OS_WIN
    static constexpr int BUTTON_WIDTH = 30;
    static constexpr int BUTTON_HEIGHT = 24;
#else
    static constexpr int BUTTON_WIDTH = 24;
    static constexpr int BUTTON_HEIGHT = 20;
#endif
};

#endif // CUSTOMTITLEBAR_H