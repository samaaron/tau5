#include "themestyles.h"
#include "../../styles/StyleManager.h"
#include "../../lib/fontloader.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QDebug>

void DebugPaneThemeStyles::applyDevToolsDarkTheme(QWebEngineView *view)
{
    if (!view || !view->page()) return;
    
    QString darkModeCSS = R"(
    (function() {
      const style = document.createElement('style');
      style.textContent = `
        /* Invert colors for dark mode */
        :root {
          filter: invert(1) hue-rotate(180deg);
          background: #1e1e1e !important;
        }
        
        /* Re-invert images and color swatches */
        img, svg, video, canvas, embed, object,
        .cm-color-swatch, .color-swatch {
          filter: invert(1) hue-rotate(180deg);
        }
        
        /* Force Cascadia Code font for all code and console elements */
        .console-message-text,
        .console-user-command,
        .console-user-command-result,
        .monospace,
        .source-code,
        .cm-s-default,
        .CodeMirror,
        .CodeMirror pre,
        .object-value-string,
        .object-value-number,
        .object-value-boolean,
        .object-value-null,
        .object-value-undefined,
        .object-value-function,
        .object-value-regexp,
        .console-formatted-string,
        .console-formatted-object,
        .console-formatted-node,
        .console-formatted-array,
        span.monospace,
        .webkit-css-property,
        .devtools-link[data-url],
        .console-message-wrapper .source-code {
          font-family: 'Cascadia Code PL', 'Cascadia Code', 'Cascadia Mono', Consolas, 'Courier New', monospace !important;
        }
        
        ::-webkit-scrollbar {
          width: 8px !important;
          height: 8px !important;
        }
        
        ::-webkit-scrollbar-track {
          background: #f0f0f0 !important;
        }
        
        ::-webkit-scrollbar-thumb {
          background: #606060 !important;
          border-radius: 0px !important;
        }
        
        ::-webkit-scrollbar-thumb:hover {
          background: #505050 !important;
        }
        
        ::-webkit-scrollbar-button {
          display: none !important;
        }
      `;
      document.head.appendChild(style);
    })();
    )";
    
    view->page()->runJavaScript(darkModeCSS);
}

void DebugPaneThemeStyles::applyLiveDashboardTau5Theme(QWebEngineView *view)
{
    if (!view || !view->page()) return;
    
    // Load CSS from resource file
    QFile cssFile(":/styles/tau5-dashboard-theme.css");
    QString cssContent;
    
    if (cssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&cssFile);
        cssContent = stream.readAll();
        cssFile.close();
    } else {
        qDebug() << "Failed to load tau5-dashboard-theme.css";
        return;
    }
    
    // Escape the CSS content for JavaScript
    cssContent.replace("\\", "\\\\");
    cssContent.replace("`", "\\`");
    cssContent.replace("$", "\\$");
    
    QString tau5CSS = QString(R"TAU5(
    (function() {
      const style = document.createElement('style');
      style.textContent = `%1`;
      document.head.appendChild(style);
    })();
    )TAU5").arg(cssContent);
    
    view->page()->runJavaScript(tau5CSS);
}

void DebugPaneThemeStyles::applyConsoleDarkTheme(QWebEngineView *view)
{
    if (!view || !view->page()) return;
    
    QString scrollbarColor = StyleManager::Colors::PRIMARY_ORANGE;
    
    QString consoleThemeCSS = QString(R"(
    (function() {
      const style = document.createElement('style');
      style.textContent = `
        body {
          background-color: #000000 !important;
          color: #ffffff !important;
          font-family: 'Cascadia Code PL', 'Cascadia Code', 'Cascadia Mono', Consolas, 'Courier New', monospace !important;
          margin: 0;
          padding: 8px;
        }
        
        pre {
          font-family: 'Cascadia Code PL', 'Cascadia Code', 'Cascadia Mono', Consolas, 'Courier New', monospace !important;
          margin: 0;
          white-space: pre-wrap;
          word-wrap: break-word;
        }
        
        /* ANSI color codes */
        .ansi-black { color: #000000; }
        .ansi-red { color: #cd0000; }
        .ansi-green { color: #00cd00; }
        .ansi-yellow { color: #cdcd00; }
        .ansi-blue { color: #0000ee; }
        .ansi-magenta { color: #cd00cd; }
        .ansi-cyan { color: #00cdcd; }
        .ansi-white { color: #e5e5e5; }
        
        .ansi-bright-black { color: #7f7f7f; }
        .ansi-bright-red { color: #ff0000; }
        .ansi-bright-green { color: #00ff00; }
        .ansi-bright-yellow { color: #ffff00; }
        .ansi-bright-blue { color: #5c5cff; }
        .ansi-bright-magenta { color: #ff00ff; }
        .ansi-bright-cyan { color: #00ffff; }
        .ansi-bright-white { color: #ffffff; }
        
        /* Background colors */
        .ansi-bg-black { background-color: #000000; }
        .ansi-bg-red { background-color: #cd0000; }
        .ansi-bg-green { background-color: #00cd00; }
        .ansi-bg-yellow { background-color: #cdcd00; }
        .ansi-bg-blue { background-color: #0000ee; }
        .ansi-bg-magenta { background-color: #cd00cd; }
        .ansi-bg-cyan { background-color: #00cdcd; }
        .ansi-bg-white { background-color: #e5e5e5; }
        
        /* Input styling */
        input, textarea {
          background-color: #1a1a1a !important;
          color: #ffffff !important;
          border: 1px solid %1 !important;
          font-family: 'Cascadia Code PL', 'Cascadia Code', 'Cascadia Mono', Consolas, 'Courier New', monospace !important;
        }
        
        input:focus, textarea:focus {
          outline: none !important;
          border-color: %1 !important;
        }
        
        /* Links */
        a {
          color: %1 !important;
          text-decoration: none;
        }
        
        a:hover {
          text-decoration: underline;
        }
        
        /* Selection */
        ::selection {
          background-color: %1 !important;
          color: #000000 !important;
        }
        
        /* Scrollbars */
        ::-webkit-scrollbar {
          width: 8px !important;
          height: 8px !important;
          background: transparent !important;
        }
        
        ::-webkit-scrollbar-track {
          background: transparent !important;
        }
        
        ::-webkit-scrollbar-thumb {
          background: %1 !important;
          border-radius: 0px !important;
        }
        
        ::-webkit-scrollbar-thumb:hover {
          background: %1 !important;
          opacity: 1.0 !important;
        }
      `;
      document.head.appendChild(style);
    })();
    )").arg(scrollbarColor);
    
    view->page()->runJavaScript(consoleThemeCSS);
}

void DebugPaneThemeStyles::injectDevToolsFontScript(QWebEngineView *view)
{
    if (!view || !view->page()) return;
    
    // Get the CSS with embedded base64 font
    QString cascadiaCodeCss = FontLoader::getCascadiaCodeCss();
    if (cascadiaCodeCss.isEmpty()) {
        qDebug() << "Failed to load Cascadia Code font for DevTools";
        return;
    }
    
    qDebug() << QString("Cascadia Code CSS size: %1 characters").arg(cascadiaCodeCss.length());
    
    // Escape the CSS for JavaScript template literal
    cascadiaCodeCss.replace("\\", "\\\\");
    cascadiaCodeCss.replace("`", "\\`");
    cascadiaCodeCss.replace("$", "\\$");
    
    QWebEngineScript fontScript;
    fontScript.setName("CascadiaCodeFont");
    fontScript.setWorldId(QWebEngineScript::ApplicationWorld);
    fontScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    fontScript.setRunsOnSubFrames(true);
    
    QString scriptSource = QString(R"SCRIPT(
    (function() {
      const observer = new MutationObserver(function(mutations) {
        // Check if document.head exists before trying to use it
        if (!document.head) {
          return;
        }
        
        const style = document.getElementById('tau5-cascadia-font') || document.createElement('style');
        style.id = 'tau5-cascadia-font';
        style.textContent = `%1`;
        
        if (!document.getElementById('tau5-cascadia-font')) {
          document.head.appendChild(style);
        }
        
        // Also inject into any shadow roots
        document.querySelectorAll('*').forEach(el => {
          if (el.shadowRoot && !el.shadowRoot.getElementById('tau5-cascadia-font-shadow')) {
            const shadowStyle = style.cloneNode(true);
            shadowStyle.id = 'tau5-cascadia-font-shadow';
            el.shadowRoot.appendChild(shadowStyle);
          }
        });
      });
      
      // Start observing
      observer.observe(document, {
        childList: true,
        subtree: true
      });
      
      // Trigger the observer callback immediately by adding a temporary element
      // This will cause the observer to fire and inject styles if document.head exists
      const temp = document.createElement('div');
      temp.style.display = 'none';
      document.documentElement.appendChild(temp);
      document.documentElement.removeChild(temp);
    })();
  )SCRIPT").arg(cascadiaCodeCss);
    
    fontScript.setSourceCode(scriptSource);
    
    // Remove any existing font script and add the new one
    QWebEngineScriptCollection &scripts = view->page()->scripts();
    QList<QWebEngineScript> existingScripts = scripts.find("CascadiaCodeFont");
    for (const QWebEngineScript &script : existingScripts) {
        scripts.remove(script);
    }
    scripts.insert(fontScript);
}

QString DebugPaneThemeStyles::getDarkScrollbarCSS()
{
    return R"(
        *::-webkit-scrollbar,
        body ::-webkit-scrollbar,
        .vbox ::-webkit-scrollbar,
        .widget ::-webkit-scrollbar,
        .console-view ::-webkit-scrollbar,
        .elements-tree-outline ::-webkit-scrollbar,
        .monospace ::-webkit-scrollbar,
        .source-code ::-webkit-scrollbar,
        .viewport ::-webkit-scrollbar,
        .scroller ::-webkit-scrollbar,
        ::-webkit-scrollbar {
          width: 8px !important;
          height: 8px !important;
          background: transparent !important;
          background-color: transparent !important;
        }
        
        *::-webkit-scrollbar-track,
        body ::-webkit-scrollbar-track,
        .vbox ::-webkit-scrollbar-track,
        .widget ::-webkit-scrollbar-track,
        .viewport ::-webkit-scrollbar-track,
        .scroller ::-webkit-scrollbar-track,
        ::-webkit-scrollbar-track {
          background: transparent !important;
          background-color: transparent !important;
          border: none !important;
          box-shadow: none !important;
        }
        
        *::-webkit-scrollbar-thumb,
        body ::-webkit-scrollbar-thumb,
        .vbox ::-webkit-scrollbar-thumb,
        .widget ::-webkit-scrollbar-thumb,
        .viewport ::-webkit-scrollbar-thumb,
        .scroller ::-webkit-scrollbar-thumb,
        ::-webkit-scrollbar-thumb {
          background: rgba(255, 165, 0, 0.941) !important;
          background-color: rgba(255, 165, 0, 0.941) !important;
          border-radius: 0px !important;
          min-height: 30px !important;
          border: none !important;
          margin: 0px !important;
          box-shadow: none !important;
        }
        
        *::-webkit-scrollbar-thumb:hover,
        body ::-webkit-scrollbar-thumb:hover,
        .vbox ::-webkit-scrollbar-thumb:hover,
        .widget ::-webkit-scrollbar-thumb:hover,
        ::-webkit-scrollbar-thumb:hover {
          background: rgba(255, 165, 0, 1.0) !important;
          background-color: rgba(255, 165, 0, 1.0) !important;
        }
        
        *::-webkit-scrollbar-corner,
        body ::-webkit-scrollbar-corner,
        ::-webkit-scrollbar-corner {
          background: transparent !important;
          background-color: transparent !important;
        }
        
        *::-webkit-scrollbar-button,
        body ::-webkit-scrollbar-button,
        ::-webkit-scrollbar-button {
          display: none !important;
          width: 0 !important;
          height: 0 !important;
        }
    )";
}