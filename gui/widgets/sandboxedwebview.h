#ifndef SANDBOXEDWEBVIEW_H
#define SANDBOXEDWEBVIEW_H

#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineDownloadRequest>

class PhxUrlInterceptor;

class SandboxedWebView : public QWebEngineView
{
    Q_OBJECT

public:
    explicit SandboxedWebView(bool devMode = false, QWidget *parent = nullptr);
    explicit SandboxedWebView(bool devMode, bool allowRemoteAccess, QWidget *parent = nullptr);
    virtual ~SandboxedWebView() = default;

    void setScrollbarColours(QColor foreground, QColor background, QColor hover);
    void insertStyleSheet(const QString &name, const QString &source);
    void setFallbackUrl(const QUrl &url) { m_fallbackUrl = url; }

protected:
    virtual void applyCustomSettings(QWebEngineSettings *settings);
    
    QWebEngineProfile *sandboxedProfile() const { return m_profile; }
    QWebEnginePage *sandboxedPage() const { return m_page; }

private slots:
    void handleDownloadRequested(QWebEngineDownloadRequest *download);

private:
    QWebEngineProfile *m_profile;
    QWebEnginePage *m_page;
    PhxUrlInterceptor *m_interceptor;
    QUrl m_fallbackUrl;
};

#endif // SANDBOXEDWEBVIEW_H