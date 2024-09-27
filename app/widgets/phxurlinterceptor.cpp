#include <iostream>
#include <QWebEngineUrlRequestInterceptor>
#include <QDesktopServices>
#include "phxurlinterceptor.h"

void PhxUrlInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{

  if (info.requestUrl().host().toStdString() != "localhost")
  {
    QDesktopServices::openUrl(info.requestUrl().url());
    info.block(true);
  }
}
