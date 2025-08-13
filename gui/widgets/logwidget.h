#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QWidget>
#include <QString>
#include <QTextCharFormat>
#include <QMap>
#include <QUrl>

QT_BEGIN_NAMESPACE
class QTextEdit;
class QVBoxLayout;
class QPushButton;
class QLineEdit;
class QShortcut;
class QTimer;
QT_END_NAMESPACE

class LogWidget : public QWidget
{
  Q_OBJECT

public:
  enum LogType {
    BeamLog,
    GuiLog,
    MCPLog
  };

  explicit LogWidget(LogType type, QWidget *parent = nullptr);
  ~LogWidget();

  // Log operations
  void appendLog(const QString &text, bool isError = false);
  void appendLogWithTimestamp(const QString &timestamp, const QString &text, bool isError = false);
  void clear();
  
  // For MCP logs that need custom formatting
  void appendFormattedText(const std::function<void(QTextCursor&)> &formatter);
  
  // Settings
  void setAutoScroll(bool enabled);
  bool autoScroll() const { return m_autoScroll; }
  
  void setMaxLines(int lines) { m_maxLines = lines; }
  int maxLines() const { return m_maxLines; }
  
  void setFontSize(int size);
  int fontSize() const { return m_fontSize; }
  
  // File operations for MCP logs
  void setLogFilePath(const QString &path);
  void startFileMonitoring(int intervalMs = 500);
  void stopFileMonitoring();
  
  // Access to text edit for external operations if needed
  QTextEdit* textEdit() { return m_textEdit; }

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
  
private:
  void setupUI();
  void setupShortcuts();
  void applyFontSize();
  void highlightAllMatches(const QString &searchText, const QTextCursor &currentMatch);
  void enforceMaxLines();
  
private:
  LogType m_type;
  QVBoxLayout *m_layout;
  QTextEdit *m_textEdit;
  
  // Search functionality
  QWidget *m_searchWidget;
  QLineEdit *m_searchInput;
  QPushButton *m_searchCloseButton;
  QString m_lastSearchText;
  QShortcut *m_searchShortcut;
  QShortcut *m_findNextShortcut;
  QShortcut *m_findPrevShortcut;
  
  // Settings
  bool m_autoScroll;
  int m_maxLines;
  int m_fontSize;
  
  // File monitoring for MCP logs
  QString m_logFilePath;
  QTimer *m_fileMonitorTimer;
  qint64 m_lastFilePosition;
  
};

#endif // LOGWIDGET_H