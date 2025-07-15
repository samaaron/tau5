#ifndef MCPSERVER_STDIO_H
#define MCPSERVER_STDIO_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMap>
#include <QTextStream>
#include <QIODevice>
#include <functional>
#include <memory>

class MCPServerStdio : public QObject
{
    Q_OBJECT

public:
    using ToolHandler = std::function<QJsonObject(const QJsonObject& params)>;

    explicit MCPServerStdio(QObject* parent = nullptr);
    ~MCPServerStdio();

    void start();
    void stop();

    struct ToolDefinition {
        QString name;
        QString description;
        QJsonObject inputSchema;
        ToolHandler handler;
    };

    void registerTool(const ToolDefinition& tool);

    void setServerInfo(const QString& name, const QString& version);
    void setCapabilities(const QJsonObject& capabilities);
    void setDebugMode(bool enabled);

signals:
    void logMessage(const QString& message);
    void stdinClosed();

private slots:
    void handleStdinReady();

private:
    void processJsonRpcRequest(const QJsonObject& request);
    
    QJsonObject handleInitialize(const QJsonObject& params);
    QJsonObject handleListTools(const QJsonObject& params);
    QJsonObject handleCallTool(const QJsonObject& params);
    
    void sendResponse(const QJsonValue& id, const QJsonObject& result);
    void sendError(const QJsonValue& id, int code, const QString& message);
    void sendNotification(const QString& method, const QJsonObject& params);
    void writeMessage(const QJsonObject& message);

private:
    QTextStream m_stdin;
    QTextStream m_stdout;
    QString m_inputBuffer;
    
    QString m_serverName;
    QString m_serverVersion;
    QJsonObject m_capabilities;
    
    QMap<QString, ToolDefinition> m_tools;
    
    bool m_initialized;
    bool m_running;
    
    static constexpr const char* JSONRPC_VERSION = "2.0";
    static constexpr const char* MCP_VERSION = "2024-11-05";
};

#endif // MCPSERVER_STDIO_H