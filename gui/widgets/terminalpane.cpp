#include "terminalpane.h"
#include "../styles/StyleManager.h"
#include "../shared/tau5logger.h"
#include <qtermwidget6/qtermwidget.h>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QProcessEnvironment>
#include <QSplitter>
#include <QEvent>
#include <QFontDatabase>
#include <QMenu>
#include <QAction>
#include <QStandardPaths>
#include <QFile>
#include <QTextStream>

TerminalPane::TerminalPane(QWidget *parent)
    : QWidget(parent)
    , m_topTerminal(nullptr)
    , m_bottomTerminal(nullptr)
    , m_activeTerminal(nullptr)
    , m_terminalSplitter(nullptr)
    , m_mainLayout(nullptr)
    , m_currentFontSize(12)
{
    setupUi();
}

TerminalPane::~TerminalPane()
{
    // Clean up terminals
    if (m_topTerminal) {
        delete m_topTerminal;
    }
    if (m_bottomTerminal) {
        delete m_bottomTerminal;
    }
}

QWidget* TerminalPane::createFontControlBar()
{
    QWidget *controlBar = new QWidget(this);
    controlBar->setMaximumHeight(28);
    controlBar->setStyleSheet(QString(
        "QWidget {"
        "  background-color: black;"
        "  border-top: 1px solid %1;"
        "}")
        .arg(StyleManager::Colors::primaryOrangeAlpha(100)));

    QHBoxLayout *layout = new QHBoxLayout(controlBar);
    layout->setContentsMargins(5, 2, 5, 2);
    layout->setSpacing(5);

    layout->addStretch();

    // Create zoom out button
    QPushButton *zoomOutButton = new QPushButton("-", controlBar);
    zoomOutButton->setToolTip("Zoom Out");
    zoomOutButton->setFixedSize(20, 20);
    zoomOutButton->setStyleSheet(QString(
        "QPushButton {"
        "  font-family: 'Segoe UI, Arial';"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  color: %1;"
        "  background: transparent;"
        "  border: none;"
        "  padding: 2px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %2;"
        "  border-radius: 3px;"
        "}")
        .arg(StyleManager::Colors::PRIMARY_ORANGE)
        .arg(StyleManager::Colors::blackAlpha(50)));
    connect(zoomOutButton, &QPushButton::clicked, this, &TerminalPane::decreaseFontSize);
    layout->addWidget(zoomOutButton);

    // Create zoom in button
    QPushButton *zoomInButton = new QPushButton("+", controlBar);
    zoomInButton->setToolTip("Zoom In");
    zoomInButton->setFixedSize(20, 20);
    zoomInButton->setStyleSheet(QString(
        "QPushButton {"
        "  font-family: 'Segoe UI, Arial';"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  color: %1;"
        "  background: transparent;"
        "  border: none;"
        "  padding: 2px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %2;"
        "  border-radius: 3px;"
        "}")
        .arg(StyleManager::Colors::PRIMARY_ORANGE)
        .arg(StyleManager::Colors::blackAlpha(50)));
    connect(zoomInButton, &QPushButton::clicked, this, &TerminalPane::increaseFontSize);
    layout->addWidget(zoomInButton);

    return controlBar;
}

void TerminalPane::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create vertical splitter for two terminals
    m_terminalSplitter = new QSplitter(Qt::Vertical, this);
    m_terminalSplitter->setChildrenCollapsible(false);

    // Create top terminal
    createTerminalWidget(m_topTerminal, true);
    m_terminalSplitter->addWidget(m_topTerminal);

    // Create bottom terminal
    createTerminalWidget(m_bottomTerminal, false);
    m_terminalSplitter->addWidget(m_bottomTerminal);

    // Set equal sizes initially
    m_terminalSplitter->setSizes(QList<int>() << 400 << 400);

    // Add splitter to main layout
    m_mainLayout->addWidget(m_terminalSplitter);

    // Add font control bar at the bottom
    QWidget *controlBar = createFontControlBar();
    m_mainLayout->addWidget(controlBar);

    // Set the active terminal to the top one initially
    m_activeTerminal = m_topTerminal;

    // Set the background color for the entire widget
    setStyleSheet(QString(
        "TerminalPane {"
        "  background-color: %1;"
        "}"
        "QSplitter::handle {"
        "  background-color: %2;"
        "  height: 3px;"
        "}"
        "QSplitter::handle:hover {"
        "  background-color: %3;"
        "}"
    ).arg(StyleManager::Colors::BACKGROUND_PRIMARY)
     .arg(StyleManager::Colors::BORDER_DEFAULT)
     .arg(StyleManager::Colors::ACCENT_PRIMARY));
}

void TerminalPane::createTerminalWidget(QTermWidget* &terminal, bool isTopTerminal)
{
    // Create QTermWidget
    terminal = new QTermWidget(0, this);

    // Install event filter to track active terminal
    terminal->installEventFilter(this);

    // Configure terminal
    terminal->setTerminalSizeHint(false);
    terminal->setTerminalOpacity(1.0);  // Ensure full opacity
    terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);

    // Create custom orange color scheme if it doesn't exist
    static bool orangeSchemeCreated = false;
    if (!orangeSchemeCreated) {
        createOrangeColorScheme();
        orangeSchemeCreated = true;
    }

    // Set our custom orange color scheme
    QStringList availableSchemes = QTermWidget::availableColorSchemes();

    if (availableSchemes.contains("OrangeOnBlack")) {
        terminal->setColorScheme("OrangeOnBlack");
    } else {
        // Fallback to a dark scheme if our custom scheme isn't available
        if (availableSchemes.contains("Linux")) {
            terminal->setColorScheme("Linux");
        } else if (availableSchemes.contains("DarkPastels")) {
            terminal->setColorScheme("DarkPastels");
        } else if (!availableSchemes.isEmpty()) {
            terminal->setColorScheme(availableSchemes.first());
        }
    }

    // Set terminal margins to reduce any extra padding
    terminal->setMargin(0);

    // Load and set Cascadia Code font from resources
    static bool cascadiaLoaded = false;
    static QString cascadiaFontFamily;

    if (!cascadiaLoaded) {
        int fontId = QFontDatabase::addApplicationFont(":/fonts/CascadiaCodePL.ttf");
        if (fontId != -1) {
            QStringList families = QFontDatabase::applicationFontFamilies(fontId);
            if (!families.isEmpty()) {
                cascadiaFontFamily = families.first();
                Tau5Logger::instance().info(QString("[TerminalPane] Loaded Cascadia font: %1").arg(cascadiaFontFamily));
                cascadiaLoaded = true;
            }
        }

        if (!cascadiaLoaded) {
            Tau5Logger::instance().error("[TerminalPane] Failed to load CascadiaCodePL.ttf from resources");
        }
    }

    QFont terminalFont(cascadiaFontFamily);
    terminalFont.setStyleHint(QFont::Monospace);
    terminalFont.setFixedPitch(true);
    terminalFont.setPointSize(m_currentFontSize);
    terminal->setTerminalFont(terminalFont);

    // Apply styling
    styleTerminal(terminal);

    // Set working directory
    if (!m_workingDirectory.isEmpty()) {
        terminal->setWorkingDirectory(m_workingDirectory);
    } else {
        terminal->setWorkingDirectory(QDir::currentPath());
    }

    // Set shell program
    QString shell = QProcessEnvironment::systemEnvironment().value("SHELL", "/bin/bash");
    terminal->setShellProgram(shell);

    // Start shell
    terminal->startShellProgram();

    // Set up context menu for copy/paste
    terminal->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(terminal, &QWidget::customContextMenuRequested,
            [this, terminal](const QPoint &pos) {
        QMenu contextMenu("Terminal Context Menu", terminal);

        QAction *copyAction = new QAction("Copy", &contextMenu);
        copyAction->setShortcut(QKeySequence::Copy);
        connect(copyAction, &QAction::triggered, [terminal]() {
            terminal->copyClipboard();
        });

        QAction *pasteAction = new QAction("Paste", &contextMenu);
        pasteAction->setShortcut(QKeySequence::Paste);
        connect(pasteAction, &QAction::triggered, [terminal]() {
            terminal->pasteClipboard();
        });

        // Only enable copy if there's a selection
        copyAction->setEnabled(terminal->selectedText().length() > 0);

        contextMenu.addAction(copyAction);
        contextMenu.addAction(pasteAction);
        contextMenu.exec(terminal->mapToGlobal(pos));
    });

    // Connect signals
    connect(terminal, &QTermWidget::finished, this, [this, terminal]() {
        // Restart the terminal when it finishes
        QString shell = QProcessEnvironment::systemEnvironment().value("SHELL", "/bin/bash");
        terminal->setShellProgram(shell);
        terminal->startShellProgram();
    });
}

void TerminalPane::styleTerminal(QTermWidget* terminal)
{
    // Apply the standard Tau5 scrollbar styling but with gray colors
    QString scrollbarStyle = QString(
        "QScrollBar:vertical { "
        "  background: transparent; "
        "  width: 8px; "
        "  border: none; "
        "  margin: 0px; "
        "}"
        "QScrollBar::handle:vertical { "
        "  background: %1; "
        "  border-radius: 0px; "
        "  min-height: 30px; "
        "  margin: 0px; "
        "  border: none; "
        "}"
        "QScrollBar::handle:vertical:hover { "
        "  background: %2; "
        "}"
        "QScrollBar::handle:vertical:pressed { "
        "  background: %2; "
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "  height: 0px; "
        "  background: transparent; "
        "  border: none; "
        "}"
        "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical { "
        "  background: transparent; "
        "  border: none; "
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
        "  background: transparent; "
        "  border: none; "
        "}")
        .arg(StyleManager::Colors::SCROLLBAR_THUMB)
        .arg(StyleManager::Colors::SCROLLBAR_THUMB_HOVER);
    terminal->setStyleSheet(scrollbarStyle);
}

void TerminalPane::setWorkingDirectory(const QString &dir)
{
    m_workingDirectory = dir;
    if (m_topTerminal) {
        m_topTerminal->setWorkingDirectory(dir);
    }
    if (m_bottomTerminal) {
        m_bottomTerminal->setWorkingDirectory(dir);
    }
}

void TerminalPane::setVisible(bool visible)
{
    QWidget::setVisible(visible);
    if (visible && m_activeTerminal) {
        m_activeTerminal->setFocus();
    }
}

void TerminalPane::handleTerminalFinished()
{
    // This is now handled inline in createTerminalWidget
}

bool TerminalPane::eventFilter(QObject *obj, QEvent *event)
{
    // Track which terminal is active based on focus events
    if (event->type() == QEvent::FocusIn) {
        if (obj == m_topTerminal) {
            m_activeTerminal = m_topTerminal;
        } else if (obj == m_bottomTerminal) {
            m_activeTerminal = m_bottomTerminal;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TerminalPane::increaseFontSize()
{
    if (m_currentFontSize < 24) {  // Max font size
        m_currentFontSize++;
        updateTerminalFonts();
    }
}

void TerminalPane::decreaseFontSize()
{
    if (m_currentFontSize > 8) {  // Min font size
        m_currentFontSize--;
        updateTerminalFonts();
    }
}

void TerminalPane::updateTerminalFonts()
{
    // Get the Cascadia font family (already loaded)
    static QString cascadiaFontFamily;
    if (cascadiaFontFamily.isEmpty()) {
        QStringList families = QFontDatabase::applicationFontFamilies(
            QFontDatabase::addApplicationFont(":/fonts/CascadiaCodePL.ttf"));
        if (!families.isEmpty()) {
            cascadiaFontFamily = families.first();
        }
    }

    QFont terminalFont(cascadiaFontFamily);
    terminalFont.setStyleHint(QFont::Monospace);
    terminalFont.setFixedPitch(true);
    terminalFont.setPointSize(m_currentFontSize);

    // Update both terminals
    if (m_topTerminal) {
        m_topTerminal->setTerminalFont(terminalFont);
    }
    if (m_bottomTerminal) {
        m_bottomTerminal->setTerminalFont(terminalFont);
    }
}

void TerminalPane::createOrangeColorScheme()
{
    // Create a temporary directory for our custom color scheme
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString colorSchemeDir = configDir + "/tau5-colorschemes";
    QDir dir;
    if (!dir.exists(colorSchemeDir)) {
        dir.mkpath(colorSchemeDir);
    }

    // Add the custom directory to QTermWidget
    QTermWidget::addCustomColorSchemeDir(colorSchemeDir);

    // Create the OrangeOnBlack.colorscheme file
    QString schemeFile = colorSchemeDir + "/OrangeOnBlack.colorscheme";
    QFile file(schemeFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);

        // Write the color scheme configuration
        stream << "[Background]\n";
        stream << "Bold=false\n";
        stream << "Color=0,0,0\n";  // Black background
        stream << "Transparency=false\n\n";

        stream << "[BackgroundIntense]\n";
        stream << "Bold=false\n";
        stream << "Color=0,0,0\n";
        stream << "Transparency=false\n\n";

        stream << "[Foreground]\n";
        stream << "Bold=false\n";
        stream << "Color=255,165,0\n";  // Orange foreground
        stream << "Transparency=false\n\n";

        stream << "[ForegroundIntense]\n";
        stream << "Bold=true\n";
        stream << "Color=255,200,0\n";  // Bright orange for intense
        stream << "Transparency=false\n\n";

        // Color palette
        stream << "[Color0]\n";  // Black
        stream << "Bold=false\n";
        stream << "Color=0,0,0\n";
        stream << "Transparency=false\n\n";

        stream << "[Color0Intense]\n";
        stream << "Bold=false\n";
        stream << "Color=104,104,104\n";
        stream << "Transparency=false\n\n";

        stream << "[Color1]\n";  // Red
        stream << "Bold=false\n";
        stream << "Color=250,75,75\n";
        stream << "Transparency=false\n\n";

        stream << "[Color1Intense]\n";
        stream << "Bold=false\n";
        stream << "Color=255,84,84\n";
        stream << "Transparency=false\n\n";

        stream << "[Color2]\n";  // Green -> Orange
        stream << "Bold=false\n";
        stream << "Color=255,140,0\n";  // Dark orange
        stream << "Transparency=false\n\n";

        stream << "[Color2Intense]\n";
        stream << "Bold=false\n";
        stream << "Color=255,200,0\n";  // Bright orange
        stream << "Transparency=false\n\n";

        stream << "[Color3]\n";  // Yellow
        stream << "Bold=false\n";
        stream << "Color=255,215,0\n";  // Gold
        stream << "Transparency=false\n\n";

        stream << "[Color3Intense]\n";
        stream << "Bold=false\n";
        stream << "Color=255,255,84\n";
        stream << "Transparency=false\n\n";

        stream << "[Color4]\n";  // Blue
        stream << "Bold=false\n";
        stream << "Color=92,167,251\n";
        stream << "Transparency=false\n\n";

        stream << "[Color4Intense]\n";
        stream << "Bold=false\n";
        stream << "Color=84,84,255\n";
        stream << "Transparency=false\n\n";

        stream << "[Color5]\n";  // Magenta
        stream << "Bold=false\n";
        stream << "Color=225,30,225\n";
        stream << "Transparency=false\n\n";

        stream << "[Color5Intense]\n";
        stream << "Bold=false\n";
        stream << "Color=255,84,255\n";
        stream << "Transparency=false\n\n";

        stream << "[Color6]\n";  // Cyan
        stream << "Bold=false\n";
        stream << "Color=24,178,178\n";
        stream << "Transparency=false\n\n";

        stream << "[Color6Intense]\n";
        stream << "Bold=false\n";
        stream << "Color=84,255,255\n";
        stream << "Transparency=false\n\n";

        stream << "[Color7]\n";  // White/Gray
        stream << "Bold=false\n";
        stream << "Color=178,178,178\n";
        stream << "Transparency=false\n\n";

        stream << "[Color7Intense]\n";
        stream << "Bold=false\n";
        stream << "Color=255,255,255\n";
        stream << "Transparency=false\n\n";

        stream << "[General]\n";
        stream << "Description=Orange on Black\n";
        stream << "Opacity=1\n";

        file.close();
        Tau5Logger::instance().info(QString("[TerminalPane] Created custom OrangeOnBlack color scheme at: %1").arg(schemeFile));
    } else {
        Tau5Logger::instance().error(QString("[TerminalPane] Failed to create color scheme file at: %1").arg(schemeFile));
    }
}