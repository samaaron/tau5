#ifndef DEBUGPANE_THEMESTYLES_H
#define DEBUGPANE_THEMESTYLES_H

#include <QString>

class QWebEngineView;
class QTextEdit;

class DebugPaneThemeStyles
{
public:
    static void applyDevToolsDarkTheme(QWebEngineView *view);
    static void applyLiveDashboardTau5Theme(QWebEngineView *view);
    static void applyConsoleDarkTheme(QWebEngineView *view);
    static void injectDevToolsFontScript(QWebEngineView *view);
    static QString getDarkScrollbarCSS();
    
private:
    static QString getDevToolsDarkThemeCSS();
    static QString getLiveDashboardThemeCSS();
    static QString getConsoleThemeCSS();
    static QString getFontScript();
};

#endif // DEBUGPANE_THEMESTYLES_H