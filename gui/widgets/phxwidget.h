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
    void handleSizeDown();
    void handleSizeUp();
    void handleOpenExternalBrowser();
    void handleResetBrowser();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QHBoxLayout *mainLayout;
    PhxWebView *phxView;
    bool phxAlive;
    QUrl defaultUrl;

private slots:
    void handleLoadFinished(bool ok);

};

#endif // PHXWIDGET_H
