#ifndef CONTROLLAYER_H
#define CONTROLLAYER_H

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>

class ControlLayer : public QWidget
{
    Q_OBJECT

public:
    explicit ControlLayer(QWidget *parent = nullptr);
    
    void setConsoleVisible(bool visible);
    void positionControls();

signals:
    void sizeDown();
    void sizeUp();
    void openExternalBrowser();
    void resetBrowser();
    void toggleConsole();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupControls();
    void connectSignals();
    
    QPushButton *m_sizeDownButton;
    QPushButton *m_sizeUpButton;
    QPushButton *m_openExternalBrowserButton;
    QPushButton *m_resetBrowserButton;
    QPushButton *m_consoleToggleButton;
    
    QHBoxLayout *m_buttonLayout;
    
    bool m_consoleVisible;
};

#endif // CONTROLLAYER_H