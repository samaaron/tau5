#include "zoomcontrol.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QTextEdit>
#include <QFont>
#include <QStringList>
#include <QScrollBar>
#include <QTextCursor>
#include <QCoreApplication>
#include <cmath>

DebugPaneZoomControl::DebugPaneZoomControl(QObject *parent)
    : QObject(parent)
{
}

void DebugPaneZoomControl::zoomWebView(QWebEngineView *view, bool zoomIn)
{
    if (!view) return;
    
    qreal currentZoom = view->zoomFactor();
    qreal newZoom = zoomIn ? currentZoom + WEBVIEW_ZOOM_STEP : currentZoom - WEBVIEW_ZOOM_STEP;
    
    // Clamp zoom between 0.5 and 3.0
    newZoom = std::max(0.5, std::min(3.0, newZoom));
    view->setZoomFactor(newZoom);
}

void DebugPaneZoomControl::zoomTextEdit(QTextEdit *textEdit, int &fontSize, bool zoomIn)
{
    if (!textEdit) return;
    
    if (zoomIn && fontSize < MAX_FONT_SIZE) {
        fontSize += FONT_SIZE_STEP;
    } else if (!zoomIn && fontSize > MIN_FONT_SIZE) {
        fontSize -= FONT_SIZE_STEP;
    }
    
    applyFontToTextEdit(textEdit, fontSize);
}

void DebugPaneZoomControl::applyFontToTextEdit(QTextEdit *textEdit, int fontSize)
{
    if (!textEdit) return;
    
    QScrollBar *vScrollBar = textEdit->verticalScrollBar();
    QScrollBar *hScrollBar = textEdit->horizontalScrollBar();
    int vScrollPos = vScrollBar->value();
    int hScrollPos = hScrollBar->value();
    QTextCursor cursor = textEdit->textCursor();
    int cursorPos = cursor.position();
    
    double vScrollPercentage = 0.0;
    if (vScrollBar->maximum() > 0) {
        vScrollPercentage = static_cast<double>(vScrollPos) / vScrollBar->maximum();
    }
    
    QStringList fontNames = {
        "Cascadia Code PL",
        "CascadiaCodePL",
        "Cascadia Code",
        "CascadiaCode",
        "Cascadia Mono",
        "Consolas",
        "Courier New"
    };
    
    QFont font;
    bool fontFound = false;
    
    for (const QString &fontName : fontNames) {
        font = QFont(fontName, fontSize);
        font.setStyleHint(QFont::Monospace);
        if (font.exactMatch() || font.family().contains("Cascadia", Qt::CaseInsensitive)) {
            fontFound = true;
            break;
        }
    }
    
    if (!fontFound) {
        font = QFont("monospace", fontSize);
        font.setStyleHint(QFont::Monospace);
    }
    
    font.setPixelSize(fontSize);
    textEdit->setFont(font);
    textEdit->document()->setDefaultFont(font);
    
    QCoreApplication::processEvents();
    
    if (vScrollBar->maximum() > 0) {
        int newVScrollPos = static_cast<int>(vScrollPercentage * vScrollBar->maximum());
        vScrollBar->setValue(newVScrollPos);
    } else {
        vScrollBar->setValue(vScrollPos);
    }
    hScrollBar->setValue(hScrollPos);
    
    cursor.setPosition(cursorPos);
    textEdit->setTextCursor(cursor);
}