#include "tau5logger.h"
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QDebug>
#include <QCoreApplication>
#include <iostream>

std::unique_ptr<Tau5Logger> Tau5Logger::s_instance = nullptr;
QMutex Tau5Logger::s_instanceMutex;

Tau5Logger::Tau5Logger() : QObject(nullptr) {
}

Tau5Logger::~Tau5Logger() {
    flush();
    closeLogFiles();
}

void Tau5Logger::initialize(const QString& appName) {
    Tau5LoggerConfig config;
    config.appName = appName;
    
    if (appName == "gui") {
        config.logFiles = {
            {"gui.log", "gui", false},
            {"beam.log", "beam", false}
        };
    } else if (appName == "mcp-gui-dev") {
        config.logFiles = {
            {"mcp.log", "mcp", true}  // JSON format for MCP
        };
    } else {
        config.logFiles = {
            {"app.log", "default", false}
        };
    }
    
    config.baseLogDir = getBaseLogDir();
    
    initialize(config);
}

void Tau5Logger::initialize(const Tau5LoggerConfig& config) {
    QMutexLocker locker(&s_instanceMutex);
    
    if (s_instance) {
        qWarning() << "Tau5Logger already initialized, ignoring re-initialization";
        return;
    }
    
    s_instance = std::make_unique<Tau5Logger>();
    s_instance->initializeWithConfig(config);
}

Tau5Logger& Tau5Logger::instance() {
    QMutexLocker locker(&s_instanceMutex);
    
    if (!s_instance) {
        qFatal("Tau5Logger not initialized! Call Tau5Logger::initialize() first.");
    }
    
    return *s_instance;
}

bool Tau5Logger::isInitialized() {
    QMutexLocker locker(&s_instanceMutex);
    return s_instance != nullptr;
}

void Tau5Logger::initializeWithConfig(const Tau5LoggerConfig& config) {
    m_config = config;
    
    if (!m_config.logFiles.isEmpty()) {
        m_defaultCategory = m_config.logFiles.first().category;
    } else {
        m_defaultCategory = "default";
    }
    
    m_sessionPath = findOrCreateSessionFolder();
    openLogFiles();
    log(LogLevel::Info, m_defaultCategory, 
        QString("Tau5Logger initialized for '%1' in session: %2")
        .arg(m_config.appName)
        .arg(m_sessionPath));
}

QString Tau5Logger::findOrCreateSessionFolder() {
    QString appDir = QString("%1/%2").arg(m_config.baseLogDir).arg(m_config.appName);
    QDir dir(appDir);
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
    QString sessionName = QString("%1_p%2").arg(timestamp).arg(QCoreApplication::applicationPid());
    QString sessionPath = dir.absoluteFilePath(sessionName);
    
    if (!dir.mkpath(sessionName)) {
        qCritical() << "Failed to create session directory:" << sessionPath;
    }
    
    cleanupOldSessions();
    
    return sessionPath;
}

void Tau5Logger::cleanupOldSessions() {
    QString appDir = QString("%1/%2").arg(m_config.baseLogDir).arg(m_config.appName);
    QDir dir(appDir);
    
    QStringList sessions = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    
    while (sessions.size() > m_config.maxSessions) {
        QString oldestSession = sessions.takeFirst();
        QString oldPath = dir.absoluteFilePath(oldestSession);
        
        QDir oldDir(oldPath);
        if (oldDir.removeRecursively()) {
            writeToConsole(LogLevel::Debug, m_defaultCategory, 
                          QString("Removed old session: %1").arg(oldestSession));
        } else {
            writeToConsole(LogLevel::Warning, m_defaultCategory,
                          QString("Failed to remove old session: %1").arg(oldestSession));
        }
    }
}

void Tau5Logger::openLogFiles() {
    for (const auto& logFile : m_config.logFiles) {
        QString filePath = QDir(m_sessionPath).absoluteFilePath(logFile.name);
        
        auto file = std::make_unique<QFile>(filePath);
        if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qCritical() << "Failed to open log file:" << filePath << file->errorString();
            continue;
        }
        
        auto stream = std::make_unique<QTextStream>(file.get());
        
        FileInfo info;
        info.file = std::move(file);
        info.stream = std::move(stream);
        info.jsonFormat = logFile.jsonFormat;
        
        m_files[logFile.category] = std::move(info);
    }
}

void Tau5Logger::closeLogFiles() {
    QMutexLocker locker(&m_mutex);
    
    for (auto& [category, info] : m_files) {
        if (info.stream) {
            info.stream->flush();
        }
        if (info.file) {
            info.file->close();
        }
    }
    m_files.clear();
}

void Tau5Logger::startNewSession() {
    QMutexLocker locker(&m_mutex);
    
    closeLogFiles();
    bool oldReuse = m_config.reuseRecentSession;
    m_config.reuseRecentSession = false;
    
    m_sessionPath = findOrCreateSessionFolder();
    m_config.reuseRecentSession = oldReuse;
    openLogFiles();
    
    log(LogLevel::Info, m_defaultCategory, 
        QString("Started new session: %1").arg(m_sessionPath));
}

QString Tau5Logger::getMCPLogPath(const QString& mcpName) const {
    return QDir(m_sessionPath).absoluteFilePath(QString("mcp-%1.log").arg(mcpName));
}

QString Tau5Logger::getGlobalMCPLogPath(const QString& mcpName) {
    QString dataPath = getTau5DataPath();
    QString mcpLogsPath = QDir(dataPath).absoluteFilePath("mcp-logs");
    
    // Ensure the mcp-logs directory exists
    QDir().mkpath(mcpLogsPath);
    
    return QDir(mcpLogsPath).absoluteFilePath(QString("mcp-%1.log").arg(mcpName));
}

QString Tau5Logger::getTau5DataPath() {
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return QDir(dataPath).absoluteFilePath("Tau5");
}

QString Tau5Logger::getBaseLogDir() {
    return QDir(getTau5DataPath()).absoluteFilePath("logs");
}

void Tau5Logger::log(LogLevel level, const QString& category, const QString& message) {
    log(level, category, message, QJsonObject());
}

void Tau5Logger::log(LogLevel level, const QString& category, const QString& message,
                     const QJsonObject& metadata) {
    if (level < m_config.minLevel) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    if (m_config.consoleEnabled) {
        writeToConsole(level, category, message);
    }
    
    // Write to file
    writeToFile(category, level, message, metadata);
    
    // Emit signal if enabled (for GUI integration)
    if (m_config.emitQtSignals) {
        emit logMessage(level, category, message, metadata);
    }
}

void Tau5Logger::writeToFile(const QString& category, LogLevel level,
                             const QString& message, const QJsonObject& metadata) {
    auto it = m_files.find(category);
    if (it == m_files.end()) {
        // If category not found, try default category
        it = m_files.find(m_defaultCategory);
        if (it == m_files.end()) {
            return;  // No file configured for this category
        }
    }
    
    auto& info = it->second;
    if (!info.stream) {
        return;
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    
    if (info.jsonFormat) {
        // Write as JSONL
        QJsonObject entry;
        entry["timestamp"] = timestamp;
        entry["level"] = levelToString(level);
        entry["category"] = category;
        entry["message"] = message;
        
        // Add metadata if present
        if (!metadata.isEmpty()) {
            for (auto it = metadata.begin(); it != metadata.end(); ++it) {
                entry[it.key()] = it.value();
            }
        }
        
        QJsonDocument doc(entry);
        *info.stream << doc.toJson(QJsonDocument::Compact) << Qt::endl;
    } else {
        // Write as plain text
        QString levelStr = levelToString(level);
        *info.stream << timestamp << " [" << levelStr << "] ";
        
        // Include category if it's not the default
        if (category != m_defaultCategory) {
            *info.stream << "[" << category << "] ";
        }
        
        *info.stream << message << Qt::endl;
    }
    
    // Flush for important messages
    if (level >= LogLevel::Warning) {
        info.stream->flush();
    }
}

void Tau5Logger::writeToConsole(LogLevel level, const QString& category, const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString levelStr = levelToString(level);
    
    QTextStream out(stderr);
    
    if (m_config.consoleColors) {
        QString colorCode = levelToColorCode(level);
        out << colorCode << timestamp << " [" << levelStr << "] ";
        
        if (category != m_defaultCategory) {
            out << "[" << category << "] ";
        }
        
        out << message << "\033[0m" << Qt::endl;  // Reset color
    } else {
        out << timestamp << " [" << levelStr << "] ";
        
        if (category != m_defaultCategory) {
            out << "[" << category << "] ";
        }
        
        out << message << Qt::endl;
    }
}

QString Tau5Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warning:  return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}

QString Tau5Logger::levelToColorCode(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:    return "\033[36m";  // Cyan
        case LogLevel::Info:     return "\033[32m";  // Green
        case LogLevel::Warning:  return "\033[33m";  // Yellow
        case LogLevel::Error:    return "\033[31m";  // Red
        case LogLevel::Critical: return "\033[35m";  // Magenta
        default:                 return "\033[0m";   // Reset
    }
}

void Tau5Logger::flush() {
    QMutexLocker locker(&m_mutex);
    
    for (auto& [category, info] : m_files) {
        if (info.stream) {
            info.stream->flush();
        }
    }
}

// Convenience methods
void Tau5Logger::debug(const QString& message) {
    log(LogLevel::Debug, m_defaultCategory, message);
}

void Tau5Logger::info(const QString& message) {
    log(LogLevel::Info, m_defaultCategory, message);
}

void Tau5Logger::warning(const QString& message) {
    log(LogLevel::Warning, m_defaultCategory, message);
}

void Tau5Logger::error(const QString& message) {
    log(LogLevel::Error, m_defaultCategory, message);
}

void Tau5Logger::critical(const QString& message) {
    log(LogLevel::Critical, m_defaultCategory, message);
}