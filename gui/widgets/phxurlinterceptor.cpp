#include <iostream>
#include <QWebEngineUrlRequestInterceptor>
#include <QDesktopServices>
#include <QDebug>
#include "phxurlinterceptor.h"

void PhxUrlInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
  QString scheme = info.requestUrl().scheme();
  QString host = info.requestUrl().host();
  int port = info.requestUrl().port();
  
  // Allow localhost and devtools
  if (host == "localhost" || host == "127.0.0.1" || host.isEmpty())
  {
    return;
  }
  
  if (scheme == "devtools")
  {
    return;
  }
  
  // In dev mode, allow WebSocket connections to DevTools port
  // This needs to be after the localhost check since DevTools might use different IPs
  if (m_devMode && (scheme == "ws" || scheme == "wss") && port == 9223)
  {
    qDebug() << "Allowing DevTools WebSocket connection:" << info.requestUrl().toString();
    return;
  }
  
  // In dev mode, log what's being blocked to help debug
  if (m_devMode)
  {
    qDebug() << "DevMode - Checking request:" << info.requestUrl().toString() 
             << "Scheme:" << scheme << "Host:" << host << "Port:" << port
             << "Type:" << info.resourceType();
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
