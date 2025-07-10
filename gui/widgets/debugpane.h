#ifndef DEBUGPANE_H
#define DEBUGPANE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QSplitter>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <memory>

class QPushButton;
class QLabel;
class QCheckBox;
class QPropertyAnimation;
class QTextEdit;
class ConsoleWidget;
class PhxWebView;

class DebugPane : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int slidePosition READ slidePosition WRITE setSlidePosition)

public:
    enum ViewMode {
        BeamLogOnly,
        DevToolsOnly,
        SideBySide
    };

    explicit DebugPane(QWidget *parent = nullptr);
    ~DebugPane();

    void appendOutput(const QString &text, bool isError = false);
    void toggle();
    bool isVisible() const { return m_isVisible; }
    void setWebView(PhxWebView *webView);
    void setViewMode(ViewMode mode);

    int slidePosition() const { return pos().y(); }
    void setSlidePosition(int pos) { move(x(), pos); }

signals:
    void visibilityChanged(bool visible);

private slots:
    void handleAutoScrollToggled(bool checked);
    void animationFinished();
    void showBeamLogOnly();
    void showDevToolsOnly();
    void showSideBySide();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void setupUi();
    void setupViewControls();
    void setupConsole();
    void setupDevTools();
    void slide(bool show);
    void updateViewMode();
    QIcon createSvgIcon(const QString &normalSvg, const QString &hoverSvg = "", const QString &selectedSvg = "");

private:
    QVBoxLayout *m_mainLayout;
    QWidget *m_headerWidget;
    QHBoxLayout *m_headerLayout;
    QStackedWidget *m_contentStack;
    QSplitter *m_splitter;

    QWidget *m_consoleWidget;
    QVBoxLayout *m_consoleLayout;
    QTextEdit *m_outputDisplay;
    QCheckBox *m_autoScrollToggle;

    QWebEngineView *m_devToolsView;
    PhxWebView *m_targetWebView;

    QPushButton *m_beamLogButton;
    QPushButton *m_devToolsButton;
    QPushButton *m_sideBySideButton;

    std::unique_ptr<QPropertyAnimation> m_slideAnimation;
    bool m_isVisible;
    bool m_autoScroll;
    int m_maxLines;
    ViewMode m_currentMode;

    bool m_isResizing;
    int m_resizeStartY;
    int m_resizeStartHeight;
    static constexpr int RESIZE_HANDLE_HEIGHT = 6;
};

#endif // DEBUGPANE_H