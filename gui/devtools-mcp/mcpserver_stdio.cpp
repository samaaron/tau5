#include "mcpserver_stdio.h"
#include "../logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QTimer>
#include <iostream>

MCPServerStdio::MCPServerStdio(QObject* parent)
    : QObject(parent)
    , m_stdin(stdin, QIODevice::ReadOnly)
    , m_stdout(stdout, QIODevice::WriteOnly)
    , m_serverName("Tau5 GUI MCP Server")
    , m_serverVersion("1.0.0")
    , m_initialized(false)
    , m_running(false)
{
    m_capabilities = QJsonObject{
        {"tools", QJsonObject{}}
    };
    
    // Make sure output is not buffered
    m_stdout.setAutoDetectUnicode(false);
}

MCPServerStdio::~MCPServerStdio()
{
    stop();
}

void MCPServerStdio::start()
{
    if (m_running) {
        return;
    }
    
    m_running = true;
    
    // Set up a timer to periodically check for stdin data
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MCPServerStdio::handleStdinReady);
    timer->start(50);
    
    emit logMessage("MCP stdio server started");
}

void MCPServerStdio::stop()
{
    m_running = false;
    emit logMessage("MCP stdio server stopped");
}

void MCPServerStdio::registerTool(const ToolDefinition& tool)
{
    m_tools[tool.name] = tool;
}

void MCPServerStdio::setServerInfo(const QString& name, const QString& version)
{
    m_serverName = name;
    m_serverVersion = version;
}

void MCPServerStdio::setCapabilities(const QJsonObject& capabilities)
{
    m_capabilities = capabilities;
}

void MCPServerStdio::handleStdinReady()
{
    if (!m_running) {
        return;
    }
    
    while (!m_stdin.atEnd()) {
        QString line = m_stdin.readLine();
        m_inputBuffer += line;
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(m_inputBuffer.toUtf8(), &error);
        
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            processJsonRpcRequest(doc.object());
            m_inputBuffer.clear();
        } else if (error.error != QJsonParseError::GarbageAtEnd && m_inputBuffer.length() < 65536) {
            sendError(QJsonValue::Null, -32700, "Parse error");
            m_inputBuffer.clear();
        }
        // Otherwise, we need more data - keep buffering
    }
}

void MCPServerStdio::processJsonRpcRequest(const QJsonObject& request)
{
    if (!request.contains("jsonrpc") || request["jsonrpc"].toString() != JSONRPC_VERSION) {
        sendError(request.value("id"), -32600, "Invalid Request");
        return;
    }
    
    if (!request.contains("method")) {
        sendError(request.value("id"), -32600, "Invalid Request");
        return;
    }
    
    QString method = request["method"].toString();
    QJsonObject params = request.value("params").toObject();
    QJsonValue id = request.value("id");
    
    try {
        QJsonObject result;
        
        if (method == "initialize") {
            result = handleInitialize(params);
            m_initialized = true;
        } else if (method == "tools/list") {
            result = handleListTools(params);
        } else if (method == "tools/call") {
            result = handleCallTool(params);
        } else if (method == "notifications/initialized") {
            return;
        } else {
            sendError(id, -32601, "Method not found");
            return;
        }
        
        if (!id.isNull()) {
            sendResponse(id, result);
        }
    } catch (const std::exception& e) {
        sendError(id, -32603, QString("Internal error: %1").arg(e.what()));
    }
}

QJsonObject MCPServerStdio::handleInitialize(const QJsonObject& params)
{
    Q_UNUSED(params);
    
    return QJsonObject{
        {"protocolVersion", MCP_VERSION},
        {"capabilities", m_capabilities},
        {"serverInfo", QJsonObject{
            {"name", m_serverName},
            {"version", m_serverVersion}
        }}
    };
}

QJsonObject MCPServerStdio::handleListTools(const QJsonObject& params)
{
    Q_UNUSED(params);
    
    QJsonArray tools;
    for (const auto& tool : m_tools) {
        tools.append(QJsonObject{
            {"name", tool.name},
            {"description", tool.description},
            {"inputSchema", tool.inputSchema}
        });
    }
    
    return QJsonObject{{"tools", tools}};
}

QJsonObject MCPServerStdio::handleCallTool(const QJsonObject& params)
{
    if (!params.contains("name")) {
        throw std::runtime_error("Missing tool name");
    }
    
    QString toolName = params["name"].toString();
    
    if (!m_tools.contains(toolName)) {
        throw std::runtime_error(QString("Unknown tool: %1").arg(toolName).toStdString());
    }
    
    const ToolDefinition& tool = m_tools[toolName];
    QJsonObject toolParams = params.value("arguments").toObject();
    
    try {
        QJsonObject result = tool.handler(toolParams);
        
        return QJsonObject{
            {"content", QJsonArray{result}},
            {"isError", false}
        };
    } catch (const std::exception& e) {
        return QJsonObject{
            {"content", QJsonArray{
                QJsonObject{
                    {"type", "text"},
                    {"text", QString("Error executing tool: %1").arg(e.what())}
                }
            }},
            {"isError", true}
        };
    }
}

void MCPServerStdio::sendResponse(const QJsonValue& id, const QJsonObject& result)
{
    QJsonObject response{
        {"jsonrpc", JSONRPC_VERSION},
        {"id", id},
        {"result", result}
    };
    
    writeMessage(response);
}

void MCPServerStdio::sendError(const QJsonValue& id, int code, const QString& message)
{
    QJsonObject response{
        {"jsonrpc", JSONRPC_VERSION},
        {"id", id},
        {"error", QJsonObject{
            {"code", code},
            {"message", message}
        }}
    };
    
    writeMessage(response);
}

void MCPServerStdio::sendNotification(const QString& method, const QJsonObject& params)
{
    QJsonObject notification{
        {"jsonrpc", JSONRPC_VERSION},
        {"method", method},
        {"params", params}
    };
    
    writeMessage(notification);
}

void MCPServerStdio::writeMessage(const QJsonObject& message)
{
    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    m_stdout << data << "\n";
    m_stdout.flush();
    
    // Also write to stderr for debugging (prefixed with #)
    std::cerr << "# MCP >> " << data.toStdString() << std::endl;
}