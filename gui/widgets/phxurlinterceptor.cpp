#include <iostream>
#include <QWebEngineUrlRequestInterceptor>
#include <QDesktopServices>
#include <QDebug>
#include "phxurlinterceptor.h"

void PhxUrlInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
  QString scheme = info.requestUrl().scheme();
  QString host = info.requestUrl().host();
  
  // Allow localhost and devtools
  if (host == "localhost" || host == "127.0.0.1" || host.isEmpty())
  {
    return;
  }
  
  if (scheme == "devtools")
  {
    return;
  }
  
  // For navigation requests, open in external browser
  if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame ||
      info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeSubFrame)
  {
    if (scheme == "http" || scheme == "https")
    {
      qDebug() << "Opening external URL in browser:" << info.requestUrl().toString();
      QDesktopServices::openUrl(info.requestUrl());
      info.block(true);
      return;
    }
  }
  
  // Block ALL external requests (images, scripts, stylesheets, etc.)
  qDebug() << "Blocking external request:" << info.requestUrl().toString() 
           << "Type:" << info.resourceType();
  info.block(true);
}
