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
  PhxUrlInterceptor(bool devMode = false, bool allowRemoteAccess = false, QObject *parent = nullptr)
    : QWebEngineUrlRequestInterceptor(parent), m_devMode(devMode), m_allowRemoteAccess(allowRemoteAccess)
  {
  }

  void interceptRequest(QWebEngineUrlRequestInfo &info);

  void setAllowRemoteAccess(bool allow) { m_allowRemoteAccess = allow; }
  bool getAllowRemoteAccess() const { return m_allowRemoteAccess; }

private:
  bool m_devMode;
  bool m_allowRemoteAccess;
};

#endif // PHXURLINTERCEPTOR_H