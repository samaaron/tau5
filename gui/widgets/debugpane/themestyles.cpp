#include "themestyles.h"
#include "../../styles/StyleManager.h"
#include "../../lib/fontloader.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QDebug>

void DebugPaneThemeStyles::applyDevToolsDarkTheme(QWebEngineView *view)
{
    if (!view || !view->page()) return;
    
    // Get the Cascadia font CSS
    QString fontCSS = FontLoader::getCascadiaCodeCss();
    QString escapedFontCSS;
    if (!fontCSS.isEmpty()) {
        escapedFontCSS = fontCSS.replace("`", "\\`").replace("$", "\\$");
    }
    
    QString darkModeCSS = QString(R"(
    (function() {
      const style = document.createElement('style');
      style.textContent = `
        %1
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
    )").arg(escapedFontCSS);
    
    view->page()->runJavaScript(darkModeCSS);
    
    QTimer::singleShot(1000, [view]() {
        if (view && view->page()) {
            injectDevToolsFontScript(view);
        }
    });
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
    
    // Get the font CSS and inject it
    QString fontCSS = FontLoader::getCascadiaCodeCss();
    if (!fontCSS.isEmpty()) {
        QString escapedFontCSS = fontCSS.replace("\\", "\\\\").replace("`", "\\`").replace("$", "\\$");
        
        QString fontInjectScript = QString(R"(
        (function() {
            // Check if font is already injected
            if (document.getElementById('tau5-cascadia-font')) return;
            
            const style = document.createElement('style');
            style.id = 'tau5-cascadia-font';
            style.textContent = `%1`;
            document.head.appendChild(style);
            
            console.log('Tau5: Injected Cascadia Code font');
        })();
        )").arg(escapedFontCSS);
        
        view->page()->runJavaScript(fontInjectScript);
    }
    
    QString fontScript = R"(
    (function() {
      function tryApplyFont() {
        const selectors = [
          '.console-message-text',
          '.console-user-command',
          '.console-user-command-result',
          '.console-formatted-object',
          '.console-formatted-string',
          '.console-formatted-number',
          '.console-formatted-null',
          '.console-formatted-undefined',
          '.console-formatted-boolean',
          '.console-formatted-function',
          '.console-formatted-regexp',
          '.console-formatted-symbol',
          '.console-formatted-node',
          '.console-formatted-array',
          '.console-group-messages',
          '.console-message',
          '.monospace',
          '.source-code',
          '.text-editor-contents',
          '.text-editor',
          '.CodeMirror',
          '.CodeMirror pre',
          '.CodeMirror-line',
          'span.monospace',
          '.object-properties-section',
          '.object-value-string',
          '.object-value-number',
          '.webkit-css-property',
          '.styles-section',
          '.devtools-link'
        ];
        
        const fontFamily = "'Cascadia Code PL', 'Cascadia Code', 'Cascadia Mono', Consolas, 'Courier New', monospace";
        
        selectors.forEach(selector => {
          try {
            const elements = document.querySelectorAll(selector);
            elements.forEach(el => {
              el.style.fontFamily = fontFamily + ' !important';
            });
          } catch (e) {}
        });
        
        const style = document.createElement('style');
        style.id = 'tau5-font-overrides';
        style.textContent = selectors.map(s => s + ' { font-family: ' + fontFamily + ' !important; }').join('\n');
        document.head.appendChild(style);
        
        // Debug: Check if font is actually loaded
        if (document.fonts && document.fonts.check) {
            const fontLoaded = document.fonts.check('16px "Cascadia Code PL"');
            console.log('Tau5: Cascadia Code PL font loaded:', fontLoaded);
        }
        
        const observer = new MutationObserver((mutations) => {
          mutations.forEach((mutation) => {
            if (mutation.type === 'childList') {
              mutation.addedNodes.forEach((node) => {
                if (node.nodeType === 1) {
                  selectors.forEach(selector => {
                    if (node.matches && node.matches(selector)) {
                      node.style.fontFamily = fontFamily + ' !important';
                    }
                    if (node.querySelectorAll) {
                      const elements = node.querySelectorAll(selector);
                      elements.forEach(el => {
                        el.style.fontFamily = fontFamily + ' !important';
                      });
                    }
                  });
                }
              });
            }
          });
        });
        
        observer.observe(document.body, {
          childList: true,
          subtree: true
        });
      }
      
      tryApplyFont();
      setTimeout(tryApplyFont, 100);
      setTimeout(tryApplyFont, 500);
      setTimeout(tryApplyFont, 1000);
      setTimeout(tryApplyFont, 2000);
    })();
    )";
    
    view->page()->runJavaScript(fontScript);
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