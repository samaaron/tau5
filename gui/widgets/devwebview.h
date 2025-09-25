#ifndef DEVWEBVIEW_H
#define DEVWEBVIEW_H

#include <QWidget>
#include <QUrl>

class SandboxedWebView;
class QMenu;
class QContextMenuEvent;
class QVBoxLayout;
class QPushButton;
class QWebEnginePage;
class QWebEngineSettings;

class DevWebView : public QWidget
{
  Q_OBJECT

public:
  explicit DevWebView(bool devMode = false, QWidget *parent = nullptr);

  // Proxy methods to access the internal web view
  SandboxedWebView* webView() const { return m_webView; }
  QWebEnginePage* page() const;
  void setUrl(const QUrl &url);
  void setFallbackUrl(const QUrl &url);
  QWebEngineSettings* settings() const;

protected:
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  void showContextMenu(const QPoint &globalPos);
  void setupZoomControls();
  void zoomIn();
  void zoomOut();

  SandboxedWebView *m_webView;
  QVBoxLayout *m_layout;
  QPushButton *m_zoomInButton;
  QPushButton *m_zoomOutButton;
};

#endif // DEVWEBVIEW_H