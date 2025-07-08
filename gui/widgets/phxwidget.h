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

private:
    QHBoxLayout *mainLayout;
    QVBoxLayout *topRowSubLayout;
    QPushButton *sizeDownButton;
    QPushButton *sizeUpButton;
    QPushButton *openExternalBrowserButton;
    QPushButton *resetBrowserButton;
    PhxWebView *phxView;
    bool phxAlive;
    QUrl defaultUrl;

private slots:
    void handleSizeDown();
    void handleOpenExternalBrowser();
    void handleSizeUp();
    void handleResetBrowser();
    void handleLoadFinished(bool ok);

};

#endif // PHXWIDGET_H
