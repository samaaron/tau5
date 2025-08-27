#include <iostream>
#include <QWebEngineUrlRequestInterceptor>
#include <QDesktopServices>
#include <QDebug>
#include "phxurlinterceptor.h"
#include "../shared/tau5logger.h"

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
    Tau5Logger::instance().debug(QString("Allowing DevTools WebSocket connection: %1").arg(info.requestUrl().toString()));
    return;
  }
  
  // In dev mode, log what's being blocked to help debug
  if (m_devMode)
  {
    Tau5Logger::instance().debug(QString("DevMode - Checking request: %1 Scheme: %2 Host: %3 Port: %4 Type: %5")
                                  .arg(info.requestUrl().toString())
                                  .arg(scheme)
                                  .arg(host)
                                  .arg(port)
                                  .arg(info.resourceType()));
  }
  
  // For navigation requests, open in external browser
  if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame ||
      info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeSubFrame)
  {
    if (scheme == "http" || scheme == "https")
    {
      Tau5Logger::instance().debug(QString("Opening external URL in browser: %1").arg(info.requestUrl().toString()));
      QDesktopServices::openUrl(info.requestUrl());
      info.block(true);
      return;
    }
  }
  
  // Block ALL external requests (images, scripts, stylesheets, etc.)
  Tau5Logger::instance().debug(QString("Blocking external request: %1 Type: %2")
                                .arg(info.requestUrl().toString())
                                .arg(info.resourceType()));
  info.block(true);
}
