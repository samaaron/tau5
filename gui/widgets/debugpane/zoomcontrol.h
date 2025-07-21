#ifndef DEBUGPANE_ZOOMCONTROL_H
#define DEBUGPANE_ZOOMCONTROL_H

#include <QObject>

class QWebEngineView;
class QTextEdit;

class DebugPaneZoomControl : public QObject
{
    Q_OBJECT

public:
    DebugPaneZoomControl(QObject *parent = nullptr);
    
    static void zoomWebView(QWebEngineView *view, bool zoomIn);
    static void zoomTextEdit(QTextEdit *textEdit, int &fontSize, bool zoomIn);
    static void applyFontToTextEdit(QTextEdit *textEdit, int fontSize);
    
    static constexpr int MIN_FONT_SIZE = 8;
    static constexpr int MAX_FONT_SIZE = 24;
    static constexpr int FONT_SIZE_STEP = 2;
    static constexpr double WEBVIEW_ZOOM_STEP = 0.1;
};

#endif // DEBUGPANE_ZOOMCONTROL_H