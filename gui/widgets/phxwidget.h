#ifndef PHXWIDGET_H
#define PHXWIDGET_H
#include <QWidget>
#include "phxwebview.h"

class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class PhxWebView;

class PhxWidget : public QWidget
{
    Q_OBJECT
public:
    PhxWidget(QWidget* parent = 0);
    void connectToTauPhx(QUrl url);
    void setConsoleVisible(bool visible);
    void raiseButtonContainer();
    void reparentButtonContainer(QWidget* newParent);
    void positionButtonContainer();

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void toggleConsole();

private:
    QHBoxLayout *mainLayout;
    QHBoxLayout *topRowSubLayout;
    QWidget *buttonContainer;
    QPushButton *sizeDownButton;
    QPushButton *sizeUpButton;
    QPushButton *openExternalBrowserButton;
    QPushButton *resetBrowserButton;
    QPushButton *consoleToggleButton;
    PhxWebView *phxView;
    bool phxAlive;
    QUrl defaultUrl;
    bool consoleVisible;

private slots:
    void handleSizeDown();
    void handleOpenExternalBrowser();
    void handleSizeUp();
    void handleResetBrowser();
    void handleLoadFinished(bool ok);
    void handleConsoleToggle();

};

#endif // PHXWIDGET_H
