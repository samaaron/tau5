#include "sandboxedwebview.h"
#include "phxurlinterceptor.h"
#include "../styles/StyleManager.h"
#include <QWebEngineSettings>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QFileDialog>
#include <QFileInfo>

SandboxedWebView::SandboxedWebView(bool devMode, QWidget *parent)
    : SandboxedWebView(devMode, false, parent)
{
}

SandboxedWebView::SandboxedWebView(bool devMode, bool allowRemoteAccess, QWidget *parent)
    : QWebEngineView(parent), m_fallbackUrl("")  // No fallback - must be set with actual port
{
    m_interceptor = new PhxUrlInterceptor(devMode, allowRemoteAccess);
    
    m_profile = new QWebEngineProfile();
    m_profile->setUrlRequestInterceptor(m_interceptor);
    
    m_page = new QWebEnginePage(m_profile);
    m_page->setParent(this);
    m_profile->setParent(this);
    
    setPage(m_page);
    setContextMenuPolicy(Qt::NoContextMenu);
    
    QWebEngineSettings *settings = m_page->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    
    // Enable developer tools in dev mode
    if (devMode) {
        settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
        settings->setAttribute(QWebEngineSettings::JavascriptCanPaste, true);
    }
    
    applyCustomSettings(settings);
    
    // Apply default Tau5 theme scrollbar colors
    // Child classes can override by calling setScrollbarColours again
    setScrollbarColours(StyleManager::Colors::ACCENT_PRIMARY, 
                       StyleManager::Colors::BACKGROUND_PRIMARY, 
                       StyleManager::Colors::ACCENT_PRIMARY);
    
    connect(m_profile, &QWebEngineProfile::downloadRequested, 
            this, &SandboxedWebView::handleDownloadRequested);
    
    // Removed automatic fallback retry - this is now handled by PhxWidget with exponential backoff
}

void SandboxedWebView::applyCustomSettings(QWebEngineSettings *settings)
{
    Q_UNUSED(settings);
}

void SandboxedWebView::handleDownloadRequested(QWebEngineDownloadRequest *download)
{
    QString filePath = QFileDialog::getSaveFileName(this, 
                                                    tr("Save File"), 
                                                    download->downloadFileName());
    
    if (!filePath.isEmpty())
    {
        QFileInfo fileInfo(filePath);
        download->setDownloadDirectory(fileInfo.absolutePath());
        download->setDownloadFileName(fileInfo.fileName());
        download->accept();
    }
}

void SandboxedWebView::setScrollbarColours(QColor foreground, QColor background, QColor hover)
{
    // Colors are unused since we're hiding the scrollbar entirely on Linux
    Q_UNUSED(foreground);
    Q_UNUSED(background);
    Q_UNUSED(hover);
    
    insertStyleSheet("scrollbar",
                     QString("/* Hide scrollbar entirely to fix Linux border issue */"
                             "::-webkit-scrollbar {"
                             "  display: none;"
                             "  width: 0;"
                             "  height: 0;"
                             "}"
                             "/* Ensure body takes full width */"
                             "body {"
                             "  width: 100% !important;"
                             "  min-width: 100% !important;"
                             "  overflow-y: auto;"
                             "  overflow-x: hidden;"
                             "}"));
}

void SandboxedWebView::insertStyleSheet(const QString &name, const QString &source)
{
    QWebEngineScript script;
    QString s = QString::fromLatin1("(function() {"
                                    "    css = document.createElement('style');"
                                    "    css.type = 'text/css';"
                                    "    css.id = '%1';"
                                    "    document.head.appendChild(css);"
                                    "    css.innerText = '%2';"
                                    "})()")
                    .arg(name)
                    .arg(source.simplified());
    
    this->page()->runJavaScript(s, QWebEngineScript::ApplicationWorld);
    
    script.setName(name);
    script.setSourceCode(s);
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setRunsOnSubFrames(true);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    this->page()->scripts().insert(script);
}

