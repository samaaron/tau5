#include "mcpserver_stdio.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QTimer>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <iostream>
#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

static bool g_debugMode = false;
static QFile* g_debugLog = nullptr;
static QTextStream* g_debugStream = nullptr;

static void debugLog(const QString& message) {
    if (!g_debugMode) return;
    
    if (g_debugStream) {
        *g_debugStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") 
                      << " | " << message << Qt::endl;
        g_debugStream->flush();
    }
    std::cerr << "# DEBUG: " << message.toStdString() << std::endl;
}

static void initDebugLogging() {
    if (g_debugMode && !g_debugLog) {
        g_debugLog = new QFile("tau5-spectra-debug.log");
        if (g_debugLog->open(QIODevice::WriteOnly | QIODevice::Append)) {
            g_debugStream = new QTextStream(g_debugLog);
            debugLog("=== MCP Server Started (DEBUG MODE) ===");
        }
    }
}

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
    
#ifdef Q_OS_WIN
    if (g_debugMode) {
        debugLog("Setting Windows stdio to binary mode");
    }
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    
    m_stdout.setAutoDetectUnicode(false);
    
    debugLog("MCPServerStdio constructed");
}

MCPServerStdio::~MCPServerStdio()
{
    debugLog("MCPServerStdio destructor called");
    stop();
    
    if (g_debugStream) {
        debugLog("=== MCP Server Stopped ===");
        delete g_debugStream;
        g_debugStream = nullptr;
    }
    if (g_debugLog) {
        g_debugLog->close();
        delete g_debugLog;
        g_debugLog = nullptr;
    }
}

void MCPServerStdio::start()
{
    if (m_running) {
        return;
    }
    
    m_running = true;
    
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

void MCPServerStdio::setDebugMode(bool enabled)
{
    g_debugMode = enabled;
    if (enabled) {
        initDebugLogging();
    }
}

void MCPServerStdio::handleStdinReady()
{
    static int callCount = 0;
    callCount++;
    
    if (!m_running) {
        debugLog("handleStdinReady called but not running");
        return;
    }
    
    if (g_debugMode && callCount % 100 == 0) {
        debugLog(QString("handleStdinReady #%1, buffer: %2 bytes").arg(callCount).arg(m_inputBuffer.length()));
    }
    
#ifdef Q_OS_WIN
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE) {
        debugLog("Windows: stdin handle is invalid");
        std::cerr << "# stdin handle is invalid, exiting..." << std::endl;
        emit stdinClosed();
        return;
    }
    
    DWORD available = 0;
    if (!PeekNamedPipe(hStdin, NULL, 0, NULL, &available, NULL)) {
        DWORD error = GetLastError();
        debugLog(QString("Windows: PeekNamedPipe failed, error %1").arg(error));
        if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
            std::cerr << "# stdin pipe is broken, exiting..." << std::endl;
            emit stdinClosed();
            return;
        }
    } else if (g_debugMode && available > 0) {
        debugLog(QString("Windows: %1 bytes available on stdin").arg(available));
    }
#else
    if (feof(stdin) || ferror(stdin)) {
        debugLog("Unix: stdin closed (feof or ferror)");
        std::cerr << "# stdin closed, exiting..." << std::endl;
        emit stdinClosed();
        return;
    }
#endif
    
    if (m_stdin.atEnd()) {
        char ch;
        if (std::cin.get(ch)) {
            std::cin.unget();
        } else {
            debugLog("EOF detected on stdin");
            std::cerr << "# EOF detected on stdin, exiting..." << std::endl;
            emit stdinClosed();
            return;
        }
    }
    
    bool dataRead = false;
    while (!m_stdin.atEnd()) {
        QString line = m_stdin.readLine();
        if (line.isNull()) {
            debugLog("Null line read from stdin");
            std::cerr << "# Null line read from stdin, exiting..." << std::endl;
            emit stdinClosed();
            return;
        }
        
        dataRead = true;
        if (g_debugMode) {
            debugLog(QString("Read %1 chars: %2").arg(line.length()).arg(line.left(100)));
        }
        
        m_inputBuffer += line;
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(m_inputBuffer.toUtf8(), &error);
        
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            debugLog(QString("Valid JSON parsed, buffer was %1 bytes").arg(m_inputBuffer.length()));
            processJsonRpcRequest(doc.object());
            m_inputBuffer.clear();
        } else if (error.error == QJsonParseError::GarbageAtEnd) {
            debugLog(QString("JSON parse: GarbageAtEnd, continuing to buffer (%1 bytes)").arg(m_inputBuffer.length()));
        } else if (m_inputBuffer.length() < 65536) {
            debugLog(QString("JSON parse error: %1 at offset %2").arg(error.errorString()).arg(error.offset));
            if (error.error != QJsonParseError::GarbageAtEnd) {
                sendError(QJsonValue::Null, -32700, "Parse error");
                m_inputBuffer.clear();
            }
        } else {
            debugLog("Buffer exceeded 65536 bytes, clearing");
            sendError(QJsonValue::Null, -32700, "Message too large");
            m_inputBuffer.clear();
        }
    }
    
    if (dataRead && g_debugMode) {
        debugLog(QString("Read cycle complete, buffer: %1 bytes").arg(m_inputBuffer.length()));
    }
}

void MCPServerStdio::processJsonRpcRequest(const QJsonObject& request)
{
    if (g_debugMode) {
        debugLog(QString("Processing request: %1").arg(QJsonDocument(request).toJson(QJsonDocument::Compact).constData()));
    }
    
    if (!request.contains("jsonrpc") || request["jsonrpc"].toString() != JSONRPC_VERSION) {
        debugLog("Invalid JSON-RPC version");
        sendError(request.value("id"), -32600, "Invalid Request");
        return;
    }
    
    if (!request.contains("method")) {
        debugLog("Missing method in request");
        sendError(request.value("id"), -32600, "Invalid Request");
        return;
    }
    
    QString method = request["method"].toString();
    QJsonObject params = request.value("params").toObject();
    QJsonValue id = request.value("id");
    
    debugLog(QString("Method: %1, ID: %2").arg(method).arg(id.isNull() ? "null" : QString::number(id.toInt())));
    
    try {
        QJsonObject result;
        
        if (method == "initialize") {
            result = handleInitialize(params);
            m_initialized = true;
            debugLog("Initialize completed");
        } else if (method == "tools/list") {
            result = handleListTools(params);
            debugLog(QString("Listed %1 tools").arg(m_tools.size()));
        } else if (method == "tools/call") {
            QString toolName = params["name"].toString();
            debugLog(QString("Calling tool: %1").arg(toolName));
            result = handleCallTool(params);
        } else if (method == "notifications/initialized") {
            debugLog("Received initialized notification");
            return;
        } else if (method == "notifications/cancelled") {
            debugLog("Received cancelled notification");
            return;
        } else {
            debugLog(QString("Unknown method: %1").arg(method));
            sendError(id, -32601, "Method not found");
            return;
        }
        
        if (!id.isNull()) {
            sendResponse(id, result);
        }
    } catch (const std::exception& e) {
        debugLog(QString("Exception: %1").arg(e.what()));
        sendError(id, -32603, QString("Internal error: %1").arg(e.what()));
    } catch (...) {
        debugLog("Unknown exception");
        sendError(id, -32603, "Unknown internal error");
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
            {"content", QJsonArray{result}}
        };
    } catch (const std::exception& e) {
        return QJsonObject{
            {"content", QJsonArray{
                QJsonObject{
                    {"type", "text"},
                    {"text", QString("Error executing tool: %1").arg(e.what())}
                }
            }}
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
    try {
        QJsonDocument doc(message);
        QByteArray data = doc.toJson(QJsonDocument::Compact);
        
        if (g_debugMode) {
            debugLog(QString("Writing %1 bytes: %2").arg(data.size()).arg(QString::fromUtf8(data).left(200)));
        }
        
        m_stdout << data << "\n";
        m_stdout.flush();
        
        if (g_debugMode) {
            debugLog(QString("Message written and flushed (%1 bytes)").arg(data.size()));
        }
        
        fflush(stdout);
        
        if (g_debugMode) {
            std::cerr << "# MCP >> " << data.toStdString() << std::endl;
        }
    } catch (const std::exception& e) {
        debugLog(QString("Exception writing message: %1").arg(e.what()));
    } catch (...) {
        debugLog("Unknown exception writing message");
    }
}