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

TerminalPane::TerminalPane(QWidget *parent)
    : QWidget(parent)
    , m_topTerminal(nullptr)
    , m_bottomTerminal(nullptr)
    , m_activeTerminal(nullptr)
    , m_terminalSplitter(nullptr)
    , m_closeButton(nullptr)
    , m_clearButton(nullptr)
    , m_copyButton(nullptr)
    , m_pasteButton(nullptr)
    , m_mainLayout(nullptr)
    , m_toolbarLayout(nullptr)
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

void TerminalPane::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create toolbar
    QWidget *toolbar = new QWidget(this);
    toolbar->setFixedHeight(28);  // Set a fixed height for the toolbar
    toolbar->setStyleSheet(QString(
        "QWidget {"
        "  background-color: %1;"
        "  border-bottom: 1px solid %2;"
        "}"
    ).arg(StyleManager::Colors::BACKGROUND_SECONDARY)
     .arg(StyleManager::Colors::BORDER_DEFAULT));

    m_toolbarLayout = new QHBoxLayout(toolbar);
    m_toolbarLayout->setContentsMargins(6, 0, 6, 0);  // Even smaller vertical padding
    m_toolbarLayout->setSpacing(4);

    // Add title label
    QLabel *titleLabel = new QLabel("Terminal", toolbar);
    titleLabel->setStyleSheet(QString(
        "QLabel {"
        "  color: %1;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "}"
    ).arg(StyleManager::Colors::TEXT_PRIMARY));
    m_toolbarLayout->addWidget(titleLabel);

    m_toolbarLayout->addStretch();

    // Create toolbar buttons
    auto createButton = [this, toolbar](const QString &text, const QString &tooltip) -> QPushButton* {
        QPushButton *btn = new QPushButton(text, toolbar);
        btn->setToolTip(tooltip);
        btn->setStyleSheet(QString(
            "QPushButton {"
            "  color: %1;"
            "  background: transparent;"
            "  border: none;"
            "  padding: 1px 6px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background-color: %2;"
            "  border-radius: 3px;"
            "}"
            "QPushButton:pressed {"
            "  background-color: %3;"
            "}"
        ).arg(StyleManager::Colors::ACCENT_PRIMARY)
         .arg(StyleManager::Colors::textPrimaryAlpha(25))
         .arg(StyleManager::Colors::textPrimaryAlpha(51)));
        return btn;
    };

    m_clearButton = createButton("Clear", "Clear terminal output");
    connect(m_clearButton, &QPushButton::clicked, this, &TerminalPane::handleClearTerminal);
    m_toolbarLayout->addWidget(m_clearButton);

    m_copyButton = createButton("Copy", "Copy selection");
    connect(m_copyButton, &QPushButton::clicked, this, &TerminalPane::handleCopySelection);
    m_toolbarLayout->addWidget(m_copyButton);

    m_pasteButton = createButton("Paste", "Paste from clipboard");
    connect(m_pasteButton, &QPushButton::clicked, this, &TerminalPane::handlePasteClipboard);
    m_toolbarLayout->addWidget(m_pasteButton);

    m_closeButton = createButton("×", "Close terminal");
    m_closeButton->setStyleSheet(m_closeButton->styleSheet() +
        "QPushButton { font-size: 18px; font-weight: bold; }");
    connect(m_closeButton, &QPushButton::clicked, this, &TerminalPane::closeRequested);
    m_toolbarLayout->addWidget(m_closeButton);

    m_mainLayout->addWidget(toolbar);

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

    // Set color scheme - try to use a dark theme
    QStringList availableSchemes = QTermWidget::availableColorSchemes();
    if (availableSchemes.contains("Linux")) {
        terminal->setColorScheme("Linux");
    } else if (availableSchemes.contains("DarkPastels")) {
        terminal->setColorScheme("DarkPastels");
    }

    // Set terminal margins to reduce any extra padding
    terminal->setMargin(0);

    // Set font using app's standard monospace font
    QFont terminalFont = QFont(StyleManager::Typography::MONOSPACE_FONT_FAMILY);
    terminalFont.setPointSize(12);  // Match app's standard font size
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

void TerminalPane::handleClearTerminal()
{
    if (m_activeTerminal) {
        m_activeTerminal->clear();
    }
}

void TerminalPane::handleCopySelection()
{
    if (m_activeTerminal) {
        m_activeTerminal->copyClipboard();
    }
}

void TerminalPane::handlePasteClipboard()
{
    if (m_activeTerminal) {
        m_activeTerminal->pasteClipboard();
    }
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