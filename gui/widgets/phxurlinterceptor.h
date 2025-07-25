#ifndef PHXURLINTERCEPTOR_H
#define PHXURLINTERCEPTOR_H

#include <QObject>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include <QtWebEngineCore/qwebengineurlrequestinterceptor.h>
#include <QDebug>

class PhxUrlInterceptor : public QWebEngineUrlRequestInterceptor
{
  Q_OBJECT
public:
  PhxUrlInterceptor(bool devMode = false, QObject *parent = nullptr) : QWebEngineUrlRequestInterceptor(parent), m_devMode(devMode)
  {
  }

  void interceptRequest(QWebEngineUrlRequestInfo &info);

private:
  bool m_devMode;
};

#endif // PHXURLINTERCEPTOR_H