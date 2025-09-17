#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include "debugwidget.h"
#include <QString>
#include <QTextCharFormat>
#include <QMap>
#include <QUrl>
#include <QMutex>

QT_BEGIN_NAMESPACE
class QTextEdit;
class QVBoxLayout;
class QPushButton;
class QLineEdit;
class QShortcut;
class QFileSystemWatcher;
QT_END_NAMESPACE

class LogWidget : public DebugWidget
{
  Q_OBJECT

public:
  enum LogType {
    BootLog,
    BeamLog,
    GuiLog,
    MCPLog
  };

  explicit LogWidget(LogType type, QWidget *parent = nullptr);
  ~LogWidget();

  void appendLog(const QString &text, bool isError = false);
  void appendLogWithTimestamp(const QString &timestamp, const QString &text, bool isError = false);
  void clear();
  void appendFormattedText(const std::function<void(QTextCursor&)> &formatter);
  void setAutoScroll(bool enabled);
  bool autoScroll() const { return m_autoScroll; }
  
  void setPaused(bool paused);
  bool isPaused() const { return m_paused; }
  bool hasPendingContent() const { return !m_pausedBuffer.isEmpty(); }
  
  void setMaxLines(int lines) { m_maxLines = lines; }
  int maxLines() const { return m_maxLines; }
  
  void setFontSize(int size);
  int fontSize() const { return m_fontSize; }
  void setLogFilePath(const QString &path);
  void stopFileMonitoring();
  QTextEdit* textEdit() { return m_textEdit; }
  void markAsRead() { m_hasUnreadContent = false; }
  bool hasUnreadContent() const { return m_hasUnreadContent; }
  bool hasNewContent() const { return m_hasUnreadContent; }
  void updateIfNeeded();

signals:
  void logActivity();

public slots:
  void zoomIn();
  void zoomOut();
  void toggleSearch();
  void findNext();
  void findPrevious();
  void handleSearchShortcut();
  
private slots:
  void performSearch();
  void closeSearch();
  void updateFromFile();
  void handleAutoScrollToggled(bool checked);
  void handlePauseToggled(bool checked);
  void onFileChanged(const QString &path);
  void onDirectoryChanged(const QString &path);
  
protected:
  void setupToolbar() override;
  void setupContent() override;
  
public:
  void onActivated() override;
  void onDeactivated() override;
  
private:
  void setupShortcuts();
  void applyFontSize();
  void highlightAllMatches(const QString &searchText, const QTextCursor &currentMatch);
  void enforceMaxLines();
  void initializeFilePosition();
  void watchForFileCreation();
  void switchFromDirectoryToFileWatch();
  
private:
  LogType m_type;
  QTextEdit *m_textEdit;
  
  QWidget *m_searchWidget;
  QLineEdit *m_searchInput;
  QPushButton *m_searchCloseButton;
  QString m_lastSearchText;
  QShortcut *m_searchShortcut;
  QShortcut *m_findNextShortcut;
  QShortcut *m_findPrevShortcut;
  bool m_autoScroll;
  bool m_paused;
  QStringList m_pausedBuffer;
  int m_pausedLineCount;
  QPushButton *m_pauseButton;
  int m_maxLines;
  int m_fontSize;
  QString m_logFilePath;
  QFileSystemWatcher *m_fileWatcher;
  QMutex m_filePositionMutex;  // Protects: m_lastFilePosition, m_lastActivityCheckPosition, m_hasUnreadContent
  bool m_isWatchingDirectory;
  qint64 m_lastFilePosition;     // Protected by m_filePositionMutex
  qint64 m_lastActivityCheckPosition;  // Protected by m_filePositionMutex
  bool m_hasUnreadContent;       // Protected by m_filePositionMutex
  
};

#endif // LOGWIDGET_H