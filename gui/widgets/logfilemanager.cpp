#include "logfilemanager.h"
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QMutexLocker>
#include <QDateTime>

LogFileManager::LogFileManager(const Config& config)
    : m_config(config)
{
    // Ensure directory exists
    QFileInfo fileInfo(m_config.filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(dir.absolutePath());
    }
}

LogFileManager::~LogFileManager()
{
    // Destructor - nothing special needed
}

bool LogFileManager::writeLine(const QString& line)
{
    if (!m_config.enabled) {
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    
    // Check if rotation is needed before writing
    rotateIfNeeded();
    
    QFile file(m_config.filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream << line << Qt::endl;
    file.close();
    
    return true;
}

bool LogFileManager::rotateIfNeeded()
{
    QFileInfo fileInfo(m_config.filePath);
    
    if (!fileInfo.exists()) {
        return false;  // No file to rotate
    }
    
    if (fileInfo.size() >= m_config.maxSizeBytes) {
        return performRotation();
    }
    
    return false;  // No rotation needed
}

bool LogFileManager::performRotation()
{
    // Close any open handles to the file
    QFile currentFile(m_config.filePath);
    
    // Rotate existing backups
    for (int i = m_config.maxBackups - 1; i >= 0; i--) {
        QString oldPath = (i == 0) 
            ? m_config.filePath 
            : m_config.filePath + "." + QString::number(i);
        QString newPath = m_config.filePath + "." + QString::number(i + 1);
        
        if (QFile::exists(oldPath)) {
            // Remove the oldest backup if it would exceed maxBackups
            if (i == m_config.maxBackups - 1) {
                QFile::remove(newPath);
            }
            // Rename to next backup number
            QFile::rename(oldPath, newPath);
        }
    }
    
    // Create a new empty log file with timestamp header
    QFile newFile(m_config.filePath);
    if (newFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&newFile);
        stream << "=== Log rotated at " 
               << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz") 
               << " ===" << Qt::endl;
        newFile.close();
        return true;
    }
    
    return false;
}