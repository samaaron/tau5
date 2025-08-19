#include "fontloader.h"
#include <QFile>
#include <QDebug>
#include "../tau5logger.h"

QString FontLoader::loadFontAsDataUri(const QString &resourcePath)
{
    QByteArray fontData = loadResourceFile(resourcePath);
    if (fontData.isEmpty()) {
        Tau5Logger::instance().warning(QString("Failed to load font from: %1").arg(resourcePath));
        return QString();
    }
    
    // Determine MIME type based on file extension
    QString mimeType = "application/font-ttf";
    if (resourcePath.endsWith(".otf", Qt::CaseInsensitive)) {
        mimeType = "font/otf";
    } else if (resourcePath.endsWith(".woff", Qt::CaseInsensitive)) {
        mimeType = "font/woff";
    } else if (resourcePath.endsWith(".woff2", Qt::CaseInsensitive)) {
        mimeType = "font/woff2";
    }
    
    return toBase64DataUri(fontData, mimeType);
}

QString FontLoader::generateFontFaceCss(const QString &fontFamily, 
                                       const QString &resourcePath,
                                       const QString &format)
{
    QString dataUri = loadFontAsDataUri(resourcePath);
    if (dataUri.isEmpty()) {
        return QString();
    }
    
    return QString("@font-face { "
                  "font-family: '%1'; "
                  "src: url(%2) format('%3'); "
                  "font-weight: normal; "
                  "font-style: normal; "
                  "font-display: swap; "
                  "}")
           .arg(fontFamily)
           .arg(dataUri)
           .arg(format);
}

QString FontLoader::getCascadiaCodeCss()
{
    // Generate @font-face rule with embedded Cascadia Code font
    QString fontFace = generateFontFaceCss("Cascadia Code PL", ":/fonts/CascadiaCodePL.ttf", "truetype");
    
    if (fontFace.isEmpty()) {
        Tau5Logger::instance().warning("Failed to generate Cascadia Code font-face CSS");
        return QString();
    }
    
    Tau5Logger::instance().debug(QString("Successfully generated Cascadia Code CSS with %1 characters").arg(fontFace.length()));
    
    // Add CSS rules that use the font
    QString css = fontFace + R"CSS(
        
        /* Apply Cascadia Code to all monospace elements */
        :root {
            --tau5-monospace-font: 'Cascadia Code PL', 'Cascadia Code', 'Cascadia Mono', Consolas, 'Courier New', monospace;
        }
        
        pre, code, .code, .console, .terminal,
        .log-output, .log-entry, .log-message,
        .code-block, .code-snippet, .highlight,
        .monospace, .mono, tt, kbd, samp,
        .process-info, .stacktrace, .traceback,
        .tau5-terminal, .tau5-terminal-input,
        .tau5-terminal-output {
            font-family: var(--tau5-monospace-font) !important;
        }
        
        /* QtWebEngine DevTools specific selectors */
        .monospace,
        .source-code,
        .cm-s-default,
        .cm-line,
        .CodeMirror,
        .CodeMirror pre,
        .CodeMirror-code,
        .console-message-text,
        .console-user-command,
        .webkit-html-attribute-value,
        .webkit-html-js-node,
        .webkit-html-css-node,
        .webkit-line-content,
        .text-editor-contents,
        .elements-disclosure li,
        .navigator-file-tree-item,
        .network-log-grid .data-grid td,
        [class*="monospace"],
        [class*="source-code"],
        [class*="console"],
        [class*="CodeMirror"] {
            font-family: var(--tau5-monospace-font) !important;
            font-size: 14px !important;
        }
    )CSS";
    
    return css;
}

QByteArray FontLoader::loadResourceFile(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Tau5Logger::instance().warning(QString("Could not open resource file: %1").arg(resourcePath));
        return QByteArray();
    }
    
    QByteArray data = file.readAll();
    Tau5Logger::instance().debug(QString("Loaded %1 bytes from %2").arg(data.size()).arg(resourcePath));
    return data;
}

QString FontLoader::toBase64DataUri(const QByteArray &data, const QString &mimeType)
{
    QString base64 = data.toBase64();
    return QString("data:%1;charset=utf-8;base64,%2")
           .arg(mimeType)
           .arg(base64);
}