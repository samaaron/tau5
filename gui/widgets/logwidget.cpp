#include "logwidget.h"
#include "../styles/StyleManager.h"
#include "../tau5logger.h"
#include <QDebug>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QShortcut>
#include <QScrollBar>
#include <QTextCursor>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <QDir>
#include <QDateTime>
#include <QLabel>
#include <QToolBar>
#include <QFontDatabase>
#include <QRegularExpression>

LogWidget::LogWidget(LogType type, QWidget *parent)
    : DebugWidget(parent)
    , m_type(type)
    , m_autoScroll(true)
    , m_maxLines(5000)
    , m_fontSize(12)
    , m_fileMonitorTimer(nullptr)
    , m_lastFilePosition(0)
    , m_lastActivityCheckPosition(0)
    , m_hasUnreadContent(false)
{
  setupUI();
  setupShortcuts();
}

LogWidget::~LogWidget()
{
  if (m_fileMonitorTimer) {
    m_fileMonitorTimer->stop();
    delete m_fileMonitorTimer;
  }
}

void LogWidget::setupToolbar()
{
  DebugWidget::setupToolbar();
  QPushButton *searchButton = createToolButton(QChar(0xEA6D), "Search (Ctrl+S)", true);
  connect(searchButton, &QPushButton::clicked, this, &LogWidget::toggleSearch);
  m_toolbarLayout->addWidget(searchButton);
  QPushButton *autoScrollButton = createToolButton(QChar(0xEA9A), "Auto-scroll", true);
  autoScrollButton->setChecked(m_autoScroll);
  connect(autoScrollButton, &QPushButton::toggled, this, &LogWidget::handleAutoScrollToggled);
  m_toolbarLayout->addWidget(autoScrollButton);
  
  m_toolbarLayout->addStretch();
  QPushButton *zoomOutButton = createToolButton("-", "Zoom Out");
  connect(zoomOutButton, &QPushButton::clicked, this, &LogWidget::zoomOut);
  m_toolbarLayout->addWidget(zoomOutButton);
  
  QPushButton *zoomInButton = createToolButton("+", "Zoom In");
  connect(zoomInButton, &QPushButton::clicked, this, &LogWidget::zoomIn);
  m_toolbarLayout->addWidget(zoomInButton);
}

void LogWidget::setupContent()
{
  DebugWidget::setupContent();
  m_textEdit = new QTextEdit(m_contentWidget);
  m_textEdit->setReadOnly(true);
  m_textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_textEdit->setStyleSheet(StyleManager::consoleOutput());
  
  applyFontSize();
  
  m_contentLayout->addWidget(m_textEdit);
  m_searchWidget = new QWidget(m_contentWidget);
  m_searchWidget->setStyleSheet(QString(
      "QWidget {"
      "  background-color: %1;"
      "  border: 1px solid %2;"
      "  border-radius: 4px;"
      "}"
      ).arg(StyleManager::Colors::blackAlpha(220))
       .arg(StyleManager::Colors::primaryOrangeAlpha(100)));
  m_searchWidget->setMaximumHeight(30);
  m_searchWidget->hide();
  
  QHBoxLayout *searchLayout = new QHBoxLayout(m_searchWidget);
  searchLayout->setContentsMargins(5, 2, 5, 2);
  
  QLabel *searchLabel = new QLabel("Find:", m_searchWidget);
  searchLabel->setStyleSheet("color: #ffffff;");
  searchLayout->addWidget(searchLabel);
  
  m_searchInput = new QLineEdit(m_searchWidget);
  m_searchInput->setStyleSheet(QString(
      "QLineEdit {"
      "  background-color: transparent;"
      "  border: none;"
      "  color: %1;"
      "  font-size: 12px;"
      "  padding: 2px 8px;"
      "}"
      "QLineEdit:focus {"
      "  outline: none;"
      "}")
      .arg(StyleManager::Colors::WHITE));
  connect(m_searchInput, &QLineEdit::textChanged, this, &LogWidget::performSearch);
  connect(m_searchInput, &QLineEdit::returnPressed, this, &LogWidget::findNext);
  searchLayout->addWidget(m_searchInput);
  
  m_searchCloseButton = new QPushButton("×", m_searchWidget);
  m_searchCloseButton->setFixedSize(20, 20);
  m_searchCloseButton->setStyleSheet(QString(
      "QPushButton {"
      "  background-color: transparent;"
      "  border: none;"
      "  color: %1;"
      "  font-size: 14px;"
      "  padding: 0px;"
      "}"
      "QPushButton:hover {"
      "  color: %2;"
      "}")
      .arg(StyleManager::Colors::primaryOrangeAlpha(150))
      .arg(StyleManager::Colors::WHITE));
  connect(m_searchCloseButton, &QPushButton::clicked, this, &LogWidget::closeSearch);
  searchLayout->addWidget(m_searchCloseButton);
  
  m_contentLayout->addWidget(m_searchWidget);
}

void LogWidget::onActivated()
{
  if (!m_logFilePath.isEmpty() && m_fileMonitorTimer && !m_fileMonitorTimer->isActive()) {
    m_fileMonitorTimer->start();
  }
  m_hasUnreadContent = false;
  m_lastActivityCheckPosition = m_lastFilePosition;
}

void LogWidget::onDeactivated()
{
  if (m_fileMonitorTimer && m_fileMonitorTimer->isActive()) {
    m_fileMonitorTimer->stop();
  }
}

void LogWidget::setupShortcuts()
{
  m_searchShortcut = new QShortcut(QKeySequence("Ctrl+S"), this);
  m_searchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(m_searchShortcut, &QShortcut::activated, this, &LogWidget::handleSearchShortcut);
  m_findPrevShortcut = new QShortcut(QKeySequence("Ctrl+R"), this);
  m_findPrevShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(m_findPrevShortcut, &QShortcut::activated, this, &LogWidget::findPrevious);
  m_findNextShortcut = new QShortcut(QKeySequence(Qt::Key_F3), this);
  m_findNextShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(m_findNextShortcut, &QShortcut::activated, this, &LogWidget::findNext);
  QShortcut *shiftF3Shortcut = new QShortcut(QKeySequence("Shift+F3"), this);
  shiftF3Shortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(shiftF3Shortcut, &QShortcut::activated, this, &LogWidget::findPrevious);
  QShortcut *closeSearchShortcut = new QShortcut(QKeySequence("Ctrl+G"), this);
  closeSearchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(closeSearchShortcut, &QShortcut::activated, this, &LogWidget::closeSearch);
  QShortcut *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), m_searchWidget);
  connect(escapeShortcut, &QShortcut::activated, this, &LogWidget::closeSearch);
}

void LogWidget::appendLog(const QString &text, bool isError)
{
  QString timestamp = QDateTime::currentDateTime().toString("[HH:mm:ss.zzz] ");
  appendLogWithTimestamp(timestamp, text, isError);
}

void LogWidget::appendLogWithTimestamp(const QString &timestamp, const QString &text, bool isError)
{
  QTextCursor cursor(m_textEdit->document());
  cursor.movePosition(QTextCursor::End);
  
  // Timestamp formatting
  QTextCharFormat timestampFormat;
  timestampFormat.setForeground(QColor(StyleManager::Colors::TIMESTAMP_GRAY));
  cursor.setCharFormat(timestampFormat);
  cursor.insertText(timestamp);
  
  // Text formatting
  QTextCharFormat textFormat;
  textFormat.setForeground(isError ? 
    QColor(StyleManager::Colors::ERROR_BLUE) : 
    QColor(StyleManager::Colors::PRIMARY_ORANGE));
  cursor.setCharFormat(textFormat);
  cursor.insertText(text);
  
  if (!text.endsWith('\n')) {
    cursor.insertText("\n");
  }
  
  enforceMaxLines();
  
  if (m_autoScroll) {
    QScrollBar *scrollBar = m_textEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
  }
  
  if (!isVisible()) {
    m_hasUnreadContent = true;
  }
  emit logActivity();
}

void LogWidget::appendFormattedText(const std::function<void(QTextCursor&)> &formatter)
{
  QTextCursor cursor(m_textEdit->document());
  cursor.movePosition(QTextCursor::End);
  
  formatter(cursor);
  
  enforceMaxLines();
  
  if (m_autoScroll) {
    QScrollBar *scrollBar = m_textEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
  }
  
  if (!isVisible()) {
    m_hasUnreadContent = true;
  }
  emit logActivity();
}

void LogWidget::clear()
{
  m_textEdit->clear();
  m_lastFilePosition = 0;
}

void LogWidget::setAutoScroll(bool enabled)
{
  m_autoScroll = enabled;
  if (enabled) {
    QScrollBar *scrollBar = m_textEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
  }
}

void LogWidget::setFontSize(int size)
{
  Tau5Logger::instance().debug(QString("LogWidget::setFontSize() called with size: %1").arg(size));
  m_fontSize = size;
  applyFontSize();
}

void LogWidget::applyFontSize()
{
  // We need to override the stylesheet's font-size
  // Get the current stylesheet and update only the font-size
  QString currentStyle = m_textEdit->styleSheet();
  
  // Use a regular expression to replace or add font-size
  // This preserves all other styling from StyleManager::textEdit()
  QString fontSizeRule = QString("font-size: %1px;").arg(m_fontSize);
  
  // First, try to replace existing font-size
  if (currentStyle.contains("font-size:")) {
    QRegularExpression re("font-size:\\s*[^;]+;");
    currentStyle.replace(re, fontSizeRule);
  } else {
    // If no font-size exists, add it to QTextEdit block
    int qtextEditPos = currentStyle.indexOf("QTextEdit {");
    if (qtextEditPos >= 0) {
      int openBrace = currentStyle.indexOf("{", qtextEditPos);
      if (openBrace >= 0) {
        currentStyle.insert(openBrace + 1, " " + fontSizeRule + " ");
      }
    }
  }
  
  m_textEdit->setStyleSheet(currentStyle);
  
  Tau5Logger::instance().debug(QString("LogWidget::applyFontSize() applied size: %1 to stylesheet").arg(m_fontSize));
}

void LogWidget::enforceMaxLines()
{
  QTextDocument *doc = m_textEdit->document();
  int blockCount = doc->blockCount();
  
  if (blockCount > m_maxLines) {
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);
    for (int i = 0; i < blockCount - m_maxLines; ++i) {
      cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
      cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
    }
    cursor.removeSelectedText();
  }
}

void LogWidget::zoomIn()
{
  Tau5Logger::instance().debug(QString("LogWidget::zoomIn() called, current fontSize: %1").arg(m_fontSize));
  setFontSize(m_fontSize + 1);
}

void LogWidget::zoomOut()
{
  Tau5Logger::instance().debug(QString("LogWidget::zoomOut() called, current fontSize: %1").arg(m_fontSize));
  if (m_fontSize > 8) {
    setFontSize(m_fontSize - 1);
  }
}

void LogWidget::toggleSearch()
{
  if (m_searchWidget->isVisible()) {
    closeSearch();
  } else {
    m_searchWidget->show();
    m_searchInput->setFocus();
    m_searchInput->selectAll();
  }
}

void LogWidget::handleSearchShortcut()
{
  if (m_searchWidget->isVisible()) {
    findNext();
  } else {
    toggleSearch();
  }
}

void LogWidget::performSearch()
{
  QString searchText = m_searchInput->text();
  
  if (searchText.isEmpty()) {
    // Clear all selections when search is empty
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.clearSelection();
    m_textEdit->setTextCursor(cursor);
    return;
  }
  
  // If search text changed, reset cursor to start
  if (searchText != m_lastSearchText) {
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_textEdit->setTextCursor(cursor);
    m_lastSearchText = searchText;
  }
  
  // Find next occurrence
  bool found = m_textEdit->find(searchText);
  
  // If not found, wrap around to beginning
  if (!found) {
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_textEdit->setTextCursor(cursor);
    found = m_textEdit->find(searchText);
  }
  
  // Highlight all matches if we found at least one
  if (found) {
    QTextCursor currentMatchCursor = m_textEdit->textCursor();
    highlightAllMatches(searchText, currentMatchCursor);
  }
}

void LogWidget::findNext()
{
  QString searchText = m_searchInput->text();
  if (searchText.isEmpty() || !m_searchWidget->isVisible()) {
    return;
  }
  
  // Find next occurrence
  bool found = m_textEdit->find(searchText);
  
  // If not found, wrap around to beginning
  if (!found) {
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_textEdit->setTextCursor(cursor);
    found = m_textEdit->find(searchText);
  }
  
  // Update highlighting if we found something
  if (found) {
    QTextCursor currentMatchCursor = m_textEdit->textCursor();
    highlightAllMatches(searchText, currentMatchCursor);
  }
}

void LogWidget::findPrevious()
{
  QString searchText = m_searchInput->text();
  if (searchText.isEmpty() || !m_searchWidget->isVisible()) {
    return;
  }
  
  // Find previous occurrence
  bool found = m_textEdit->find(searchText, QTextDocument::FindBackward);
  
  // If not found, wrap around to end
  if (!found) {
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_textEdit->setTextCursor(cursor);
    found = m_textEdit->find(searchText, QTextDocument::FindBackward);
  }
  
  // Update highlighting if we found something
  if (found) {
    QTextCursor currentMatchCursor = m_textEdit->textCursor();
    highlightAllMatches(searchText, currentMatchCursor);
  }
}

void LogWidget::highlightAllMatches(const QString &searchText, const QTextCursor &currentMatch)
{
  QList<QTextEdit::ExtraSelection> extraSelections;
  QTextDocument *document = m_textEdit->document();
  QTextCursor highlightCursor(document);
  
  // Setup the format for other occurrences (orange background)
  QTextEdit::ExtraSelection extraSelection;
  QTextCharFormat format;
  format.setBackground(QColor(StyleManager::Colors::PRIMARY_ORANGE));
  format.setForeground(QColor(StyleManager::Colors::BLACK));
  
  // Find all occurrences and highlight them (except the current one)
  while (!highlightCursor.isNull() && !highlightCursor.atEnd()) {
    highlightCursor = document->find(searchText, highlightCursor);
    if (!highlightCursor.isNull()) {
      // Only add if it's not the current selection
      if (!currentMatch.isNull() && 
          (highlightCursor.position() != currentMatch.position() || 
           highlightCursor.anchor() != currentMatch.anchor())) {
        extraSelection.cursor = highlightCursor;
        extraSelection.format = format;
        extraSelections.append(extraSelection);
      }
    }
  }
  
  // Apply all selections
  m_textEdit->setExtraSelections(extraSelections);
}

void LogWidget::closeSearch()
{
  m_searchWidget->hide();
  m_searchInput->clear();
  m_lastSearchText.clear();
  
  // Clear selection and extra selections (highlights)
  QTextCursor cursor = m_textEdit->textCursor();
  cursor.clearSelection();
  m_textEdit->setTextCursor(cursor);
  m_textEdit->setExtraSelections(QList<QTextEdit::ExtraSelection>());
  m_textEdit->setFocus();
}

void LogWidget::setLogFilePath(const QString &path)
{
  m_logFilePath = path;
  m_lastFilePosition = 0;
  m_lastActivityCheckPosition = 0;
}

void LogWidget::startFileMonitoring(int intervalMs)
{
  if (!m_fileMonitorTimer) {
    m_fileMonitorTimer = new QTimer(this);
    connect(m_fileMonitorTimer, &QTimer::timeout, this, &LogWidget::updateFromFile);
  }
  
  if (m_type == MCPLog && !m_logFilePath.isEmpty()) {
    QFile logFile(m_logFilePath);
    
    if (logFile.exists() && logFile.open(QIODevice::ReadOnly)) {
      m_lastFilePosition = logFile.size();
      m_lastActivityCheckPosition = m_lastFilePosition;
      logFile.close();
      Tau5Logger::instance().debug(QString("MCP log monitoring starting from position: %1").arg(m_lastFilePosition));
    } else {
      m_lastFilePosition = 0;
      m_lastActivityCheckPosition = 0;
      Tau5Logger::instance().debug("MCP log doesn't exist yet, will start from beginning");
    }
  }
  
  m_fileMonitorTimer->setInterval(intervalMs);
  m_fileMonitorTimer->start();
  
  if (m_type != MCPLog) {
    updateFromFile();
  }
}

void LogWidget::stopFileMonitoring()
{
  if (m_fileMonitorTimer) {
    m_fileMonitorTimer->stop();
  }
}

void LogWidget::updateFromFile()
{
  if (m_logFilePath.isEmpty()) {
    return;
  }
  
  QFile logFile(m_logFilePath);
  if (!logFile.exists()) {
    return;
  }
  
  if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }
  
  // Check if file has changed
  qint64 currentSize = logFile.size();
  if (currentSize == m_lastFilePosition) {
    logFile.close();
    return;
  }
  
  // Read from last position
  if (m_lastFilePosition > 0 && m_lastFilePosition < currentSize) {
    logFile.seek(m_lastFilePosition);
  } else if (m_lastFilePosition > currentSize) {
    // File was truncated or replaced (e.g., server restart)
    // Reset to beginning and re-read entire file
    m_lastFilePosition = 0;
    logFile.seek(0);
    clear();
  } else {
    // First read
    clear();
  }
  
  QTextStream stream(&logFile);
  
  // For MCP logs, parse JSON and format
  if (m_type == MCPLog) {
    appendFormattedText([this, &stream](QTextCursor &cursor) {
      // Set up text formats
      QTextCharFormat timestampFormat;
      timestampFormat.setForeground(QColor(StyleManager::Colors::TIMESTAMP_GRAY));
      
      QTextCharFormat normalFormat;
      normalFormat.setForeground(QColor(StyleManager::Colors::PRIMARY_ORANGE));
      
      QTextCharFormat successFormat;
      successFormat.setForeground(QColor(StyleManager::Colors::STATUS_SUCCESS));
      
      QTextCharFormat errorFormat;
      errorFormat.setForeground(QColor(StyleManager::Colors::ERROR_BLUE));
      
      while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (line.isEmpty()) continue;
        
        // Parse JSON log entry
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
        
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
          QJsonObject entry = doc.object();
          
          // Extract fields
          QString timestamp = entry["timestamp"].toString();
          QString tool = entry["tool"].toString();
          QString status = entry["status"].toString();
          int duration = entry["duration_ms"].toInt(-1);
          QJsonObject params = entry["params"].toObject();
          
          // Format timestamp
          if (timestamp.contains("T")) {
            timestamp = timestamp.mid(timestamp.indexOf("T") + 1, 12);
          }
          
          // Handle session markers specially
          if (tool == "_session") {
            QTextCharFormat sessionFormat;
            sessionFormat.setForeground(QColor(StyleManager::Colors::ACCENT_HIGHLIGHT));
            sessionFormat.setFontWeight(QFont::Bold);
            
            QString sessionId = entry["session_id"].toString();
            qint64 pid = entry["pid"].toInteger();
            
            cursor.insertText("\n");
            cursor.setCharFormat(sessionFormat);
            cursor.insertText("════════════════════════════════════════════════════════════\n");
            cursor.insertText(QString("  NEW SESSION - %1\n").arg(timestamp));
            if (!sessionId.isEmpty()) {
              cursor.insertText(QString("  Session ID: %1  PID: %2\n").arg(sessionId).arg(pid));
            }
            cursor.insertText("════════════════════════════════════════════════════════════\n");
            cursor.insertText("\n");
          } else {
            // Regular tool entry
            // Determine format based on status (use error color for error/exception/crash)
            QTextCharFormat lineFormat = (status == "error" || status == "exception" || status == "crash") 
                ? errorFormat : normalFormat;
            
            cursor.setCharFormat(timestampFormat);
            cursor.insertText(QString("[%1] ").arg(timestamp));
            
            cursor.setCharFormat(lineFormat);
            cursor.insertText(QString("%1 ").arg(tool));
            
            // Status indicator
            QString statusStr;
            if (status == "started") {
              statusStr = "→";
            } else if (status == "success") {
              statusStr = "✓";
            } else if (status == "error") {
              statusStr = "✗";
            } else {
              statusStr = status;
            }
            
            cursor.setCharFormat(lineFormat);
            cursor.insertText(statusStr);
            
            if (duration >= 0) {
              cursor.setCharFormat(lineFormat);
              cursor.insertText(QString(" (%1ms)").arg(duration));
            }
            
            // Parameters (only show for 'started' status or when there's no error)
            if (!params.isEmpty() && status != "error" && status != "exception" && status != "crash") {
              QJsonDocument paramsDoc(params);
              QString paramsStr = paramsDoc.toJson(QJsonDocument::Compact);
              cursor.setCharFormat(lineFormat);
              if (paramsStr.length() > 200) {
                QString truncated = paramsStr.left(197) + "...";
                cursor.insertText(QString("\n  %1").arg(truncated));
              } else {
                cursor.insertText(QString("\n  %1").arg(paramsStr));
              }
            }
            
            // Response data (for successful calls)
            if (status == "success" && entry.contains("response")) {
              QJsonValue response = entry["response"];
              QString responseStr;
              
              if (response.isString()) {
                responseStr = response.toString();
              } else if (response.isObject() || response.isArray()) {
                QJsonDocument responseDoc;
                if (response.isObject()) {
                  responseDoc = QJsonDocument(response.toObject());
                } else {
                  responseDoc = QJsonDocument(response.toArray());
                }
                responseStr = responseDoc.toJson(QJsonDocument::Compact);
              } else if (response.isDouble()) {
                responseStr = QString::number(response.toDouble());
              } else if (response.isBool()) {
                responseStr = response.toBool() ? "true" : "false";
              } else if (response.isNull()) {
                responseStr = "null";
              }
              
              QTextCharFormat responseFormat;
              responseFormat.setForeground(QColor(StyleManager::Colors::STATUS_SUCCESS));
              cursor.setCharFormat(responseFormat);
              
              // Truncate response if too long
              if (responseStr.length() > 300) {
                QString truncated = responseStr.left(297) + "...";
                cursor.insertText(QString("\n  → %1").arg(truncated));
              } else {
                cursor.insertText(QString("\n  → %1").arg(responseStr));
              }
            }
            
            // Error details (for error/exception/crash statuses)
            QString errorMsg = entry["error"].toString();
            if (!errorMsg.isEmpty() && (status == "error" || status == "exception" || status == "crash")) {
              cursor.setCharFormat(lineFormat);  // Use same color as rest of error line
              // Replace newlines with spaces for compact display
              errorMsg = errorMsg.replace('\n', ' ');
              
              // Truncate error message if too long
              if (errorMsg.length() > 200) {
                QString truncated = errorMsg.left(197) + "...";
                cursor.insertText(QString("\n  Error: %1").arg(truncated));
              } else {
                cursor.insertText(QString("\n  Error: %1").arg(errorMsg));
              }
            }
            
            cursor.insertText("\n");
          }
        } else {
          // Raw line if not JSON
          cursor.setCharFormat(normalFormat);
          cursor.insertText(line + "\n");
        }
      }
    });
  } else {
    // For regular logs, just append the text
    while (!stream.atEnd()) {
      QString line = stream.readLine();
      appendLog(line, false);
    }
  }
  
  m_lastFilePosition = logFile.pos();
  logFile.close();
}

void LogWidget::handleAutoScrollToggled(bool checked)
{
  setAutoScroll(checked);
}

bool LogWidget::checkForNewContent()
{
  if (m_logFilePath.isEmpty()) {
    return false;
  }
  
  QFile logFile(m_logFilePath);
  if (!logFile.exists()) {
    return false;
  }
  
  qint64 currentSize = logFile.size();
  if (currentSize != m_lastActivityCheckPosition) {
    m_lastActivityCheckPosition = currentSize;
    m_hasUnreadContent = true;
    emit logActivity();
    return true;
  }
  
  return false;
}

void LogWidget::updateIfNeeded()
{
  if (checkForNewContent()) {
    updateFromFile();
  }
}
