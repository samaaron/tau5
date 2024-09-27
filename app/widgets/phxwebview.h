#ifndef PHXWEBVIEW_H
#define PHXWEBVIEW_H

#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEngineDownloadRequest>

class QWebEngineProfile;
class QWebEnginePage;
class QWebEngineDownloadRequest;

class PhxWebView : public QWebEngineView
{
  Q_OBJECT

public:
  explicit PhxWebView(QWidget *parent = nullptr);
  void setScrollbarColours(QColor foreground, QColor background, QColor hover);

private slots:
  void handleDownloadRequested(QWebEngineDownloadRequest *download);

private:
  void insertStyleSheet(const QString &name, const QString &source);

  QWebEngineProfile *phxProfile;
  QWebEnginePage *phxPage;
};

#endif // PHXWEBVIEW_H
