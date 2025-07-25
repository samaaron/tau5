#include "sandboxedwebview.h"
#include "phxurlinterceptor.h"
#include "../styles/StyleManager.h"
#include <QWebEngineSettings>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QFileDialog>
#include <QFileInfo>

SandboxedWebView::SandboxedWebView(bool devMode, QWidget *parent)
    : QWebEngineView(parent), m_fallbackUrl("http://localhost:5555")
{
    m_interceptor = new PhxUrlInterceptor(devMode);
    
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
    insertStyleSheet("scrollbar",
                     QString("/* Width */"
                             "::-webkit-scrollbar {"
                             "  width: 5px;"
                             "}"
                             "/* Track */"
                             "::-webkit-scrollbar-track {"
                             "    background: %1;"
                             "}"
                             "/* Thumb */"
                             "::-webkit-scrollbar-thumb {"
                             "  border-radius: 8px;"
                             "  background: %2;"
                             "}"
                             "/* Thumb on hover */"
                             "::-webkit-scrollbar-thumb:hover {"
                             "  background: %3;"
                             "}")
                         .arg(background.name())
                         .arg(foreground.name())
                         .arg(hover.name()));
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

