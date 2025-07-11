#include <iostream>
#include <QWebEngineUrlRequestInterceptor>
#include <QDesktopServices>
#include <QDebug>
#include "phxurlinterceptor.h"

void PhxUrlInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
  if (info.resourceType() != QWebEngineUrlRequestInfo::ResourceTypeMainFrame &&
      info.resourceType() != QWebEngineUrlRequestInfo::ResourceTypeSubFrame)
  {
    return;
  }

  QString scheme = info.requestUrl().scheme();
  QString host = info.requestUrl().host();
  
  if (host == "localhost" || host == "127.0.0.1" || host.isEmpty())
  {
    return;
  }
  
  if (scheme == "devtools")
  {
    return;
  }
  
  if (scheme == "http" || scheme == "https")
  {
    qDebug() << "Opening external URL in browser:" << info.requestUrl().toString();
    QDesktopServices::openUrl(info.requestUrl());
    info.block(true);
    return;
  }
  
  qDebug() << "Blocking navigation to:" << info.requestUrl().toString();
  info.block(true);
}
