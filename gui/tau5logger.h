#ifndef TAU5LOGGER_H
#define TAU5LOGGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QVector>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <memory>
#include <unordered_map>

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4
};

struct Tau5LoggerConfig {
    QString appName;                    // Required: "gui" or "mcp-gui-dev"
    QString baseLogDir;                 // Default: ~/.local/share/Tau5/logs
    int maxSessions = 5;                // Keep last 5 session folders
    bool reuseRecentSession = true;    // Reuse session if < 1 hour old
    int sessionReuseHours = 1;         // How old a session can be to reuse
    
    struct LogFile {
        QString name;                   // e.g., "gui.log", "beam.log"
        QString category;               // Category that maps to this file
        bool jsonFormat = false;        // Plain text or JSONL
    };
    QVector<LogFile> logFiles;          // Multiple logs per session
    
    bool consoleEnabled = true;         
    bool consoleColors = true;
    bool emitQtSignals = false;        // For GUI integration
    LogLevel minLevel = LogLevel::Debug;
};

class Tau5Logger : public QObject {
    Q_OBJECT
    
public:
    // Simple initialization - just app name, defaults for everything else
    static void initialize(const QString& appName);
    
    // Full configuration
    static void initialize(const Tau5LoggerConfig& config);
    
    // Get the singleton instance
    static Tau5Logger& instance();
    
    // Check if initialized
    static bool isInitialized();
    
    // Logging methods with category
    void log(LogLevel level, const QString& category, const QString& message);
    void log(LogLevel level, const QString& category, const QString& message, 
             const QJsonObject& metadata);
    
    // Convenience methods that use default category (first configured file)
    void debug(const QString& message);
    void info(const QString& message);
    void warning(const QString& message);
    void error(const QString& message);
    void critical(const QString& message);
    
    // Force start a new session (new timestamped folder)
    void startNewSession();
    
    // Get current session path
    QString currentSessionPath() const { return m_sessionPath; }
    
    // Flush all file buffers
    void flush();
    
signals:
    void logMessage(LogLevel level, const QString& category, 
                   const QString& message, const QJsonObject& metadata);
    
public:
    Tau5Logger();
    ~Tau5Logger();
    
private:
    
    void initializeWithConfig(const Tau5LoggerConfig& config);
    QString findOrCreateSessionFolder();
    void cleanupOldSessions();
    void openLogFiles();
    void closeLogFiles();
    void writeToFile(const QString& category, LogLevel level, 
                    const QString& message, const QJsonObject& metadata);
    void writeToConsole(LogLevel level, const QString& category, const QString& message);
    QString levelToString(LogLevel level) const;
    QString levelToColorCode(LogLevel level) const;
    
    Tau5LoggerConfig m_config;
    QString m_sessionPath;
    QString m_defaultCategory;
    mutable QMutex m_mutex;
    
    // Map category to file stream
    struct FileInfo {
        std::unique_ptr<QFile> file;
        std::unique_ptr<QTextStream> stream;
        bool jsonFormat;
    };
    std::unordered_map<QString, FileInfo> m_files;
    
    static std::unique_ptr<Tau5Logger> s_instance;
    static QMutex s_instanceMutex;
};

// Convenience macros for compatibility
#define TAU5_LOG_DEBUG(msg) Tau5Logger::instance().debug(msg)
#define TAU5_LOG_INFO(msg) Tau5Logger::instance().info(msg)
#define TAU5_LOG_WARNING(msg) Tau5Logger::instance().warning(msg)
#define TAU5_LOG_ERROR(msg) Tau5Logger::instance().error(msg)
#define TAU5_LOG_CRITICAL(msg) Tau5Logger::instance().critical(msg)

#endif // TAU5LOGGER_H