#ifndef PHXWIDGET_H
#define PHXWIDGET_H
#include <QWidget>
#include <QDateTime>
#include "phxwebview.h"

class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class PhxWebView;
class QTimer;

class PhxWidget : public QWidget
{
  Q_OBJECT
public:
  PhxWidget(bool devMode = false, QWidget *parent = 0);
  void connectToTauPhx(QUrl url);
  void handleSizeDown();
  void handleSizeUp();
  void handleOpenExternalBrowser();
  void handleResetBrowser();
  
  PhxWebView* getWebView() const { return phxView; }

signals:
  void pageLoaded();

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  QHBoxLayout *mainLayout;
  PhxWebView *phxView;
  bool phxAlive;
  QUrl defaultUrl;
  bool m_devMode;
  
  // Retry limiting members
  int retryCount;
  QDateTime lastRetryTime;
  QTimer *retryTimer;
  static const int MAX_RETRIES = 5;
  static const int INITIAL_RETRY_DELAY_MS = 1000; // 1 second

private slots:
  void handleLoadFinished(bool ok);
  void performRetry();
};

#endif // PHXWIDGET_H
