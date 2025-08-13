#ifndef LOGFILEMANAGER_H
#define LOGFILEMANAGER_H

#include <QString>
#include <QFile>
#include <QMutex>

class LogFileManager
{
public:
    struct Config {
        QString filePath;
        qint64 maxSizeBytes = 10 * 1024 * 1024;  // Default 10MB
        int maxBackups = 1;  // Number of backup files to keep
        bool enabled = true;
    };
    
    explicit LogFileManager(const Config& config);
    ~LogFileManager();
    
    // Thread-safe log writing with automatic rotation
    bool writeLine(const QString& line);
    
    // Manual rotation trigger
    bool rotateIfNeeded();
    
    // Get current file path
    QString currentFilePath() const { return m_config.filePath; }
    
private:
    bool performRotation();
    
    Config m_config;
    mutable QMutex m_mutex;  // For thread safety
};

#endif // LOGFILEMANAGER_H