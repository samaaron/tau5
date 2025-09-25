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
class QWebChannel;
class Tau5DevBridge;

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
  void handleSaveAsImage();
  
  PhxWebView* getWebView() const { return phxView; }

signals:
  void pageLoaded();
  void appPageReady();  // Emitted when the main app page is ready
  void webViewRecreated();  // Emitted after hard refresh when new web view is created

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
  bool m_allowRemoteAccess;
  QWebChannel *m_webChannel;
  Tau5DevBridge *m_devBridge;

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

private:
  void setupWebChannel();
};

#endif // PHXWIDGET_H
