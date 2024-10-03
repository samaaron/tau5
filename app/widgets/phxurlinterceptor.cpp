#include <iostream>
#include <QWebEngineUrlRequestInterceptor>
#include <QDesktopServices>
#include "phxurlinterceptor.h"

void PhxUrlInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{


  if ((info.requestUrl().host().toStdString() != "localhost") && (info.requestUrl().host().toStdString() != "127.0.0.1"))
  {
    QDesktopServices::openUrl(info.requestUrl().url());
    info.block(true);
  }
}
