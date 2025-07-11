#ifndef DEBUGPANE_H
#define DEBUGPANE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QTabWidget>
#include <QStackedWidget>
#include <memory>

class QPushButton;
class QLabel;
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
    void appendGuiLog(const QString &text, bool isError = false);
    void toggle();
    bool isVisible() const { return m_isVisible; }
    void setWebView(PhxWebView *webView);
    void setViewMode(ViewMode mode);
    void setLiveDashboardUrl(const QString &url);
    void saveSettings();
    void restoreSettings();

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
    void handleZoomIn();
    void handleZoomOut();
    void handleConsoleZoomIn();
    void handleConsoleZoomOut();
    void handleGuiLogZoomIn();
    void handleGuiLogZoomOut();
    void showBeamLog();
    void showGuiLog();
    void showDevToolsTab();
    void showLiveDashboardTab();

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
    void applyDevToolsDarkTheme();
    QIcon createSvgIcon(const QString &normalSvg, const QString &hoverSvg = "", const QString &selectedSvg = "");
    QPixmap createSvgPixmap(const QString &svg, int width, int height);
    QWidget* createTabToolbar(QWidget *parent);
    QString getTabButtonStyle();
    QString getZoomButtonStyle();
    QPushButton* createTabButton(const QString &text, QWidget *parent);
    QPushButton* createZoomButton(const QIcon &icon, const QString &tooltip, QWidget *parent);

private:
    QVBoxLayout *m_mainLayout;
    QWidget *m_headerWidget;
    QHBoxLayout *m_headerLayout;
    QSplitter *m_splitter;

    QWidget *m_consoleContainer;
    QStackedWidget *m_consoleStack;
    QPushButton *m_beamLogTabButton;
    QPushButton *m_guiLogTabButton;
    QWidget *m_beamLogContainer;
    QVBoxLayout *m_beamLogLayout;
    QTextEdit *m_outputDisplay;
    QPushButton *m_autoScrollButton;
    QPushButton *m_consoleZoomInButton;
    QPushButton *m_consoleZoomOutButton;
    QPushButton *m_devToolsZoomInButton;
    QPushButton *m_devToolsZoomOutButton;
    QWidget *m_guiLogContainer;
    QVBoxLayout *m_guiLogLayout;
    QTextEdit *m_guiLogDisplay;
    QPushButton *m_guiLogAutoScrollButton;
    QPushButton *m_guiLogZoomInButton;
    QPushButton *m_guiLogZoomOutButton;
    QTabWidget *m_devToolsTabs;
    QWidget *m_devToolsMainContainer;
    QStackedWidget *m_devToolsStack;
    QPushButton *m_devToolsTabButton;
    QPushButton *m_liveDashboardTabButton;
    QWidget *m_devToolsContainer;
    QWebEngineView *m_devToolsView;
    QPushButton *m_zoomInButton;
    QPushButton *m_zoomOutButton;
    QWidget *m_liveDashboardContainer;
    QWebEngineView *m_liveDashboardView;
    QPushButton *m_liveDashboardZoomInButton;
    QPushButton *m_liveDashboardZoomOutButton;
    
    PhxWebView *m_targetWebView;
    int m_currentFontSize;
    int m_guiLogFontSize;

    QPushButton *m_beamLogButton;
    QPushButton *m_devToolsButton;
    QPushButton *m_sideBySideButton;

    std::unique_ptr<QPropertyAnimation> m_slideAnimation;
    bool m_isVisible;
    bool m_autoScroll;
    bool m_guiLogAutoScroll;
    int m_maxLines;
    ViewMode m_currentMode;

    bool m_isResizing;
    int m_resizeStartY;
    int m_resizeStartHeight;
    static constexpr int RESIZE_HANDLE_HEIGHT = 6;
};

#endif // DEBUGPANE_H