#include "logwidget.h"
#include "../styles/StyleManager.h"
#include "../shared/tau5logger.h"
#include <QDebug>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QShortcut>
#include <QScrollBar>
#include <QTextCursor>
#include <QFileSystemWatcher>
#include <QMutexLocker>
#include <QFile>
#include <QThread>
#include <QPointer>
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
    , m_paused(false)
    , m_pausedLineCount(0)
    , m_pauseButton(nullptr)
    , m_maxLines(5000)
    , m_fontSize(12)
    , m_fileWatcher(nullptr)
    , m_isWatchingDirectory(false)
    , m_lastFilePosition(0)
    , m_lastActivityCheckPosition(0)
    , m_hasUnreadContent(false)
{
  setupUI();
  setupShortcuts();
}

LogWidget::~LogWidget()
{
  if (m_fileWatcher) {
    m_fileWatcher->deleteLater();
  }
}

void LogWidget::setupToolbar()
{
  DebugWidget::setupToolbar();
  QPushButton *searchButton = createToolButton(QChar(0xEA6D), "Search (Ctrl+S)", true);
  connect(searchButton, &QPushButton::clicked, this, &LogWidget::toggleSearch);
  m_toolbarLayout->addWidget(searchButton);

  if (m_type != BootLog) {
    m_pauseButton = createToolButton(QChar(0xEB2B), "Pause log updates", true);
    m_pauseButton->setChecked(m_paused);
    connect(m_pauseButton, &QPushButton::toggled, this, &LogWidget::handlePauseToggled);
    m_toolbarLayout->addWidget(m_pauseButton);

    QPushButton *autoScrollButton = createToolButton(QChar(0xEA9A), "Auto-scroll", true);
    autoScrollButton->setChecked(m_autoScroll);
    connect(autoScrollButton, &QPushButton::toggled, this, &LogWidget::handleAutoScrollToggled);
    m_toolbarLayout->addWidget(autoScrollButton);

    QPushButton *clearButton = createToolButton(QChar(0xEA81), "Clear log");
    connect(clearButton, &QPushButton::clicked, this, &LogWidget::clear);
    m_toolbarLayout->addWidget(clearButton);
  }

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
  m_hasUnreadContent = false;
  m_lastActivityCheckPosition = m_lastFilePosition;
}

void LogWidget::onDeactivated()
{
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
  if (m_paused) {
    QString fullLine = timestamp + text;
    if (!text.endsWith('\n')) {
      fullLine += '\n';
    }
    m_pausedBuffer.append(fullLine);
    m_pausedLineCount++;
    
    if (m_pauseButton) {
      m_pauseButton->setToolTip(QString("Resume log updates (%1 lines buffered)").arg(m_pausedLineCount));
    }
    
    emit logActivity();
    return;
  }
  
  QTextCursor cursor(m_textEdit->document());
  cursor.movePosition(QTextCursor::End);
  
  QTextCharFormat timestampFormat;
  timestampFormat.setForeground(QColor(StyleManager::Colors::TIMESTAMP_GRAY));
  cursor.setCharFormat(timestampFormat);
  cursor.insertText(timestamp);
  
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
  if (m_paused) {
    QTextDocument tempDoc;
    QTextCursor tempCursor(&tempDoc);
    formatter(tempCursor);
    
    QString bufferedText = tempDoc.toPlainText();
    if (!bufferedText.isEmpty()) {
      m_pausedBuffer.append(bufferedText);
      m_pausedLineCount += bufferedText.count('\n');
      if (!bufferedText.endsWith('\n')) {
        m_pausedLineCount++;
      }
      
      if (m_pauseButton) {
        m_pauseButton->setToolTip(QString("Resume log updates (%1 lines buffered)").arg(m_pausedLineCount));
      }
    }
    
    emit logActivity();
    return;
  }
  
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
  QString currentStyle = m_textEdit->styleSheet();
  
  QString fontSizeRule = QString("font-size: %1px;").arg(m_fontSize);
  
  if (currentStyle.contains("font-size:")) {
    QRegularExpression re("font-size:\\s*[^;]+;");
    currentStyle.replace(re, fontSizeRule);
  } else {
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
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.clearSelection();
    m_textEdit->setTextCursor(cursor);
    return;
  }
  
  if (searchText != m_lastSearchText) {
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_textEdit->setTextCursor(cursor);
    m_lastSearchText = searchText;
  }
  
  bool found = m_textEdit->find(searchText);
  
  if (!found) {
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_textEdit->setTextCursor(cursor);
    found = m_textEdit->find(searchText);
  }
  
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
  
  bool found = m_textEdit->find(searchText);
  
  if (!found) {
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_textEdit->setTextCursor(cursor);
    found = m_textEdit->find(searchText);
  }
  
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
  
  bool found = m_textEdit->find(searchText, QTextDocument::FindBackward);
  
  if (!found) {
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_textEdit->setTextCursor(cursor);
    found = m_textEdit->find(searchText, QTextDocument::FindBackward);
  }
  
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
  
  QTextEdit::ExtraSelection extraSelection;
  QTextCharFormat format;
  format.setBackground(QColor(StyleManager::Colors::PRIMARY_ORANGE));
  format.setForeground(QColor(StyleManager::Colors::BLACK));
  
  while (!highlightCursor.isNull() && !highlightCursor.atEnd()) {
    highlightCursor = document->find(searchText, highlightCursor);
    if (!highlightCursor.isNull()) {
      if (!currentMatch.isNull() && 
          (highlightCursor.position() != currentMatch.position() || 
           highlightCursor.anchor() != currentMatch.anchor())) {
        extraSelection.cursor = highlightCursor;
        extraSelection.format = format;
        extraSelections.append(extraSelection);
      }
    }
  }
  
  m_textEdit->setExtraSelections(extraSelections);
}

void LogWidget::closeSearch()
{
  m_searchWidget->hide();
  m_searchInput->clear();
  m_lastSearchText.clear();
  
  QTextCursor cursor = m_textEdit->textCursor();
  cursor.clearSelection();
  m_textEdit->setTextCursor(cursor);
  m_textEdit->setExtraSelections(QList<QTextEdit::ExtraSelection>());
  m_textEdit->setFocus();
}

void LogWidget::setLogFilePath(const QString &path)
{
  if (m_fileWatcher) {
    disconnect(m_fileWatcher, nullptr, this, nullptr);
    m_fileWatcher->deleteLater();
    m_fileWatcher = nullptr;
  }
  
  {
    QMutexLocker locker(&m_filePositionMutex);
    m_logFilePath = path;
    m_lastFilePosition = 0;
    m_lastActivityCheckPosition = 0;
    m_hasUnreadContent = false;
  }
  
  if (path.isEmpty()) {
    return;
  }
  
  m_fileWatcher = new QFileSystemWatcher(this);
  
  connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
          this, &LogWidget::onFileChanged);
  connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
          this, &LogWidget::onDirectoryChanged);
  
  QFileInfo fileInfo(m_logFilePath);
  
  if (fileInfo.exists()) {
    bool added = m_fileWatcher->addPath(m_logFilePath);
    
    if (fileInfo.isSymLink()) {
      QString targetPath = fileInfo.canonicalFilePath();
      if (!targetPath.isEmpty() && targetPath != m_logFilePath) {
        m_fileWatcher->addPath(targetPath);
        Tau5Logger::instance().debug(QString("Watching symlink %1 and target %2").arg(m_logFilePath).arg(targetPath));
      }
    }
    
    if (!added) {
      Tau5Logger::instance().warning(QString("Failed to add file watcher for: %1").arg(m_logFilePath));
    }
    m_isWatchingDirectory = false;
    initializeFilePosition();
  } else {
    watchForFileCreation();
  }
}


void LogWidget::stopFileMonitoring()
{
  if (m_fileWatcher) {
    m_fileWatcher->deleteLater();
    m_fileWatcher = nullptr;
  }
}

void LogWidget::updateFromFile()
{
  if (m_logFilePath.isEmpty()) {
    return;
  }
  
  qint64 readPosition;
  bool shouldClear = false;
  {
    QMutexLocker locker(&m_filePositionMutex);
    readPosition = m_lastFilePosition;
  }
  
  QFile logFile(m_logFilePath);
  if (!logFile.exists()) {
    return;
  }
  
  if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    Tau5Logger::instance().warning(QString("Failed to open log file for reading: %1").arg(m_logFilePath));
    return;
  }
  
  qint64 currentSize = logFile.size();
  if (currentSize == readPosition) {
    logFile.close();
    return;
  }
  
  if (readPosition > 0 && readPosition < currentSize) {
    logFile.seek(readPosition);
  } else if (readPosition > currentSize) {
    readPosition = 0;
    logFile.seek(0);
    shouldClear = true;
  } else {
    shouldClear = true;
  }
  
  if (shouldClear) {
    clear();
  }
  
  QTextStream stream(&logFile);
  
  if (m_type == MCPLog) {
    appendFormattedText([this, &stream](QTextCursor &cursor) {
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
        
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
        
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
          QJsonObject entry = doc.object();
          
          QString timestamp = entry["timestamp"].toString();
          QString tool = entry["tool"].toString();
          QString status = entry["status"].toString();
          int duration = entry["duration_ms"].toInt(-1);
          QJsonObject params = entry["params"].toObject();
          
          if (timestamp.contains("T")) {
            timestamp = timestamp.mid(timestamp.indexOf("T") + 1, 12);
          }
          
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
            QTextCharFormat lineFormat = (status == "error" || status == "exception" || status == "crash") 
                ? errorFormat : normalFormat;
            
            cursor.setCharFormat(timestampFormat);
            cursor.insertText(QString("[%1] ").arg(timestamp));
            
            cursor.setCharFormat(lineFormat);
            cursor.insertText(QString("%1 ").arg(tool));
            
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
              
              if (responseStr.length() > 300) {
                QString truncated = responseStr.left(297) + "...";
                cursor.insertText(QString("\n  → %1").arg(truncated));
              } else {
                cursor.insertText(QString("\n  → %1").arg(responseStr));
              }
            }
            
            QString errorMsg = entry["error"].toString();
            if (!errorMsg.isEmpty() && (status == "error" || status == "exception" || status == "crash")) {
              cursor.setCharFormat(lineFormat);  // Use same color as rest of error line
              errorMsg = errorMsg.replace('\n', ' ');
              
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
          cursor.setCharFormat(normalFormat);
          cursor.insertText(line + "\n");
        }
      }
    });
  } else {
    while (!stream.atEnd()) {
      QString line = stream.readLine();
      appendLog(line, false);
    }
  }
  
  qint64 newPosition = logFile.pos();
  logFile.close();
  
  {
    QMutexLocker locker(&m_filePositionMutex);
    m_lastFilePosition = newPosition;
    m_hasUnreadContent = false;
  }
}

void LogWidget::handleAutoScrollToggled(bool checked)
{
  setAutoScroll(checked);
}

void LogWidget::handlePauseToggled(bool checked)
{
  setPaused(checked);
}

void LogWidget::setPaused(bool paused)
{
  if (m_paused == paused) {
    return;
  }
  
  m_paused = paused;
  
  if (!m_paused) {
    if (!m_pausedBuffer.isEmpty()) {
      QTextCursor cursor(m_textEdit->document());
      cursor.movePosition(QTextCursor::End);
      
      QTextCharFormat separatorFormat;
      separatorFormat.setForeground(QColor(StyleManager::Colors::ACCENT_HIGHLIGHT));
      cursor.setCharFormat(separatorFormat);
      cursor.insertText(QString("\n══════ %1 lines buffered while paused ══════\n").arg(m_pausedLineCount));
      
      QTextCharFormat normalFormat;
      normalFormat.setForeground(QColor(StyleManager::Colors::PRIMARY_ORANGE));
      cursor.setCharFormat(normalFormat);
      
      for (const QString &bufferedLine : m_pausedBuffer) {
        cursor.insertText(bufferedLine);
      }
      
      m_pausedBuffer.clear();
      m_pausedLineCount = 0;
      
      enforceMaxLines();
      
      if (m_autoScroll) {
        QScrollBar *scrollBar = m_textEdit->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
      }
    }
  }
  
  if (m_pauseButton) {
    if (m_paused) {
      if (m_pausedLineCount > 0) {
        m_pauseButton->setToolTip(QString("Resume log updates (%1 lines buffered)").arg(m_pausedLineCount));
      } else {
        m_pauseButton->setToolTip("Resume log updates");
      }
    } else {
      m_pauseButton->setToolTip("Pause log updates");
    }
  }
}

void LogWidget::updateIfNeeded()
{
  if (m_hasUnreadContent) {
    updateFromFile();
  }
}

void LogWidget::onFileChanged(const QString &path)
{
  bool shouldEmitActivity = false;
  bool shouldUpdateDisplay = false;
  bool fileReplaced = false;
  
  {
    QMutexLocker locker(&m_filePositionMutex);
    
    QFileInfo fileInfo(path);
    QString actualPath = fileInfo.canonicalFilePath();
    if (actualPath.isEmpty()) {
      actualPath = path;
    }
    
    if (actualPath == m_logFilePath || path == m_logFilePath) {
      QFile file(m_logFilePath);
      if (file.open(QIODevice::ReadOnly)) {
        qint64 newSize = file.size();
        
        if (newSize < m_lastActivityCheckPosition && m_lastActivityCheckPosition > 0) {
          Tau5Logger::instance().debug(QString("File replaced detected: %1 (old size: %2, new size: %3)")
            .arg(path)
            .arg(m_lastActivityCheckPosition)
            .arg(newSize));
          
          m_lastFilePosition = 0;
          m_lastActivityCheckPosition = newSize;
          fileReplaced = true;
          m_hasUnreadContent = true;
          shouldEmitActivity = true;
        } else if (newSize > m_lastActivityCheckPosition) {
          
          m_lastActivityCheckPosition = newSize;
          m_hasUnreadContent = true;
          shouldEmitActivity = true;
        }
        
        if (m_hasUnreadContent) {
          shouldUpdateDisplay = true;
        }
        
        file.close();
      } else {
        Tau5Logger::instance().debug(QString("Failed to open file, might be recreated: %1").arg(path));
      }
    }
  }
  QPointer<LogWidget> safeThis(this);
  if (shouldEmitActivity && safeThis) {
    emit logActivity();
  }
  
  if (shouldUpdateDisplay && safeThis) {
    QMetaObject::invokeMethod(this, "updateFromFile", Qt::QueuedConnection);
  }
  
  if (m_fileWatcher && !m_fileWatcher->files().contains(path)) {
    int retryCount = 0;
    const int maxRetries = 3;
    while (retryCount < maxRetries) {
      bool added = m_fileWatcher->addPath(path);
      if (added) {
        if (retryCount > 0) {
          Tau5Logger::instance().debug(QString("Successfully re-added file watcher for %1 after %2 retries")
            .arg(path).arg(retryCount));
        }
        break;
      }
      retryCount++;
      if (retryCount < maxRetries && QFile::exists(path)) {
        QThread::msleep(10);
      }
    }
    if (retryCount == maxRetries && QFile::exists(path)) {
      Tau5Logger::instance().warning(QString("Failed to re-add file watcher for %1 after %2 retries")
        .arg(path).arg(maxRetries));
    }
  }
}

void LogWidget::onDirectoryChanged(const QString &path)
{
  if (!m_isWatchingDirectory) {
    return;
  }
  
  QFileInfo fileInfo(m_logFilePath);
  if (fileInfo.exists()) {
    Tau5Logger::instance().debug(QString("Target log file created: %1").arg(m_logFilePath));
    switchFromDirectoryToFileWatch();
  } else {
    QFileInfo dirInfo(path);
    if (dirInfo.exists()) {
      Tau5Logger::instance().debug(QString("Directory changed but target file not yet created: %1")
        .arg(fileInfo.fileName()));
    }
  }
}

void LogWidget::initializeFilePosition()
{
  QMutexLocker locker(&m_filePositionMutex);
  
  QFile logFile(m_logFilePath);
  if (logFile.exists() && logFile.open(QIODevice::ReadOnly)) {
    m_lastFilePosition = logFile.size();
    m_lastActivityCheckPosition = m_lastFilePosition;
    logFile.close();
  } else {
    m_lastFilePosition = 0;
    m_lastActivityCheckPosition = 0;
  }
}

void LogWidget::watchForFileCreation()
{
  if (!m_fileWatcher) {
    Tau5Logger::instance().warning("Cannot watch for file creation: file watcher is null");
    return;
  }
  
  QFileInfo fileInfo(m_logFilePath);
  QString dirPath = fileInfo.absolutePath();
  
  if (QDir(dirPath).exists()) {
    bool added = m_fileWatcher->addPath(dirPath);
    if (added) {
      m_isWatchingDirectory = true;
      Tau5Logger::instance().debug(QString("Watching directory for file creation: %1").arg(dirPath));
    } else {
      Tau5Logger::instance().warning(QString("Failed to watch directory: %1").arg(dirPath));
    }
  } else {
    Tau5Logger::instance().warning(QString("Cannot watch for file creation: directory does not exist: %1").arg(dirPath));
  }
}

void LogWidget::switchFromDirectoryToFileWatch()
{
  if (!m_fileWatcher) return;
  
  QFileInfo fileInfo(m_logFilePath);
  QString dirPath = fileInfo.absolutePath();
  bool removed = m_fileWatcher->removePath(dirPath);
  if (!removed) {
    Tau5Logger::instance().debug(QString("Directory was not being watched: %1").arg(dirPath));
  }
  
  bool added = m_fileWatcher->addPath(m_logFilePath);
  if (added) {
    m_isWatchingDirectory = false;
    Tau5Logger::instance().debug(QString("Switched to watching file: %1").arg(m_logFilePath));
  } else {
    Tau5Logger::instance().warning(QString("Failed to watch file after creation: %1").arg(m_logFilePath));
  }
  
  initializeFilePosition();
  
  QMetaObject::invokeMethod(this, "updateFromFile", Qt::QueuedConnection);
}
