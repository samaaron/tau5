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
  PhxWidget(bool devMode, bool allowRemoteAccess, QWidget *parent = 0);
  void connectToTauPhx(QUrl url);
  void handleSizeDown();
  void handleSizeUp();
  void handleOpenExternalBrowser();
  void handleResetBrowser();
  
  PhxWebView* getWebView() const { return phxView; }

signals:
  void pageLoaded();
  void appPageReady();  // Emitted when the main app page is ready

protected:
  void resizeEvent(QResizeEvent *event) override;
  void setPhxAlive(bool alive) { phxAlive = alive; }
  PhxWebView* getPhxView() const { return phxView; }

private:
  QHBoxLayout *mainLayout;
  PhxWebView *phxView;
  bool phxAlive;
  QUrl defaultUrl;
  bool m_devMode;
  
  int retryCount;
  QDateTime lastRetryTime;
  QTimer *retryTimer;
  QTimer *appPageTimer;
  bool appPageEmitted;
  static constexpr int MAX_RETRIES = 5;
  static constexpr int INITIAL_RETRY_DELAY_MS = 1000;

private slots:
  void handleLoadFinished(bool ok);
  void performRetry();
};

#endif // PHXWIDGET_H
