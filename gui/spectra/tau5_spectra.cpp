#include <QCoreApplication>
#include <QTimer>
#include <QEventLoop>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QUuid>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <iostream>
#include <memory>
#include <algorithm>
#include "mcpserver_stdio.h"
#include "../shared/tau5logger.h"
#include "cdpclient.h"
#include "tidewaveproxy.h"

static void debugLog(const QString& message) {
    std::cerr << "# " << message.toStdString() << std::endl;
}

class MCPActivityLogger
{
public:
    MCPActivityLogger(const QString& logName) {
        m_logPath = Tau5Logger::getGlobalMCPLogPath(logName);
        m_processId = QCoreApplication::applicationPid();
        
        // Generate a unique session ID for this connection
        m_sessionId = QString("%1_%2").arg(m_processId).arg(QDateTime::currentDateTime().toString("HHmmss"));
        
        rotateLogIfNeeded();
        writeSessionMarker();
    }
    
    void logActivity(const QString& tool, const QString& requestId, const QJsonObject& params, 
                     const QString& status, qint64 durationMs, const QString& errorDetails = QString(),
                     const QJsonValue& responseData = QJsonValue()) {
        QJsonObject entry;
        entry["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        entry["session_id"] = m_sessionId;
        entry["pid"] = m_processId;
        entry["tool"] = tool;
        entry["request_id"] = requestId;
        entry["params"] = params;
        
        QJsonDocument paramsDoc(params);
        entry["params_size"] = static_cast<int>(paramsDoc.toJson(QJsonDocument::Compact).size());
        
        entry["status"] = status;
        entry["duration_ms"] = durationMs;
        
        if (!errorDetails.isEmpty() && (status == "error" || status == "exception" || status == "crash")) {
            entry["error"] = errorDetails;
        }
        if (!responseData.isNull() && !responseData.isUndefined() && status == "success") {
            entry["response"] = responseData;
            
            QJsonDocument responseDoc;
            if (responseData.isObject()) {
                responseDoc = QJsonDocument(responseData.toObject());
            } else if (responseData.isArray()) {
                responseDoc = QJsonDocument(responseData.toArray());
            } else {
                QJsonObject wrapper;
                wrapper["value"] = responseData;
                responseDoc = QJsonDocument(wrapper);
            }
            entry["response_size"] = static_cast<int>(responseDoc.toJson(QJsonDocument::Compact).size());
        }
        
        writeLogEntry(entry);
    }
    
private:
    void rotateLogIfNeeded() {
        QFileInfo fileInfo(m_logPath);
        if (fileInfo.exists() && fileInfo.size() > 10 * 1024 * 1024) { // 10MB limit
            QString rotatedPath = m_logPath + "." + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
            QFile::rename(m_logPath, rotatedPath);
            
            QDir logsDir = fileInfo.dir();
            QStringList filters;
            filters << fileInfo.fileName() + ".*";
            QFileInfoList rotatedFiles = logsDir.entryInfoList(filters, QDir::Files, QDir::Time);
            
            while (rotatedFiles.size() > 5) {
                QFile::remove(rotatedFiles.last().absoluteFilePath());
                rotatedFiles.removeLast();
            }
        }
    }
    
    void writeSessionMarker() {
        QJsonObject sessionEntry;
        sessionEntry["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        sessionEntry["session_id"] = m_sessionId;
        sessionEntry["pid"] = m_processId;
        sessionEntry["tool"] = "_session";
        sessionEntry["status"] = "started";
        QJsonObject params;
        params["type"] = "mcp_server_session";
        params["session_id"] = m_sessionId;
        params["pid"] = m_processId;
        sessionEntry["params"] = params;
        
        writeLogEntry(sessionEntry);
    }
    
    void writeLogEntry(const QJsonObject& entry) {
        QFile file(m_logPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Unbuffered)) {
            QJsonDocument doc(entry);
            QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
            file.write(data);
            file.close();
        }
    }
    
    QString m_logPath;
    QString m_sessionId;
    qint64 m_processId;
};

class TidewaveBridge : public QObject
{
    Q_OBJECT

public:
    explicit TidewaveBridge(TidewaveProxy* proxy, QObject* parent = nullptr)
        : QObject(parent), m_proxy(proxy) {}

    QJsonObject executeCommand(const QString& toolName, const QJsonObject& params)
    {
        if (!m_proxy->isAvailable()) {
            return QJsonObject{
                {"error", true},
                {"message", "Tidewave MCP server is not available"}
            };
        }

        QJsonObject result;
        QString error;
        QEventLoop loop;

        m_proxy->callTool(toolName, params, [&result, &error, &loop](const QJsonObject& res, const QString& err) {
            result = res;
            error = err;
            loop.quit();
        });

        QTimer timeout;
        timeout.setSingleShot(true);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(30000); // 30 second timeout

        loop.exec();

        if (!error.isEmpty()) {
            return QJsonObject{
                {"error", true},
                {"message", error}
            };
        }

        return result;
    }

    QJsonObject formatResponse(const QJsonObject& result)
    {
        // Tidewave returns content array with text
        if (result.contains("content")) {
            QJsonArray content = result["content"].toArray();
            if (!content.isEmpty() && content[0].toObject().contains("text")) {
                return QJsonObject{
                    {"type", "text"},
                    {"text", content[0].toObject()["text"].toString()}
                };
            }
        }

        return QJsonObject{
            {"type", "text"},
            {"text", QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Indented))}
        };
    }

private:
    TidewaveProxy* m_proxy;
};

class CDPBridge : public QObject
{
    Q_OBJECT
    
public:
    explicit CDPBridge(CDPClient* client, QObject* parent = nullptr)
        : QObject(parent), m_client(client) {}
    
    bool waitForConnection(int timeoutMs = 5000)
    {
        if (m_client->isConnected()) {
            return true;
        }
        
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        timeout.setInterval(timeoutMs);
        
        bool connected = false;
        
        auto connHandler = connect(m_client, &CDPClient::connected, [&]() {
            connected = true;
            loop.quit();
        });
        
        auto discHandler = connect(m_client, &CDPClient::disconnected, [&]() {
            loop.quit();
        });
        
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        timeout.start();
        loop.exec();
        
        disconnect(connHandler);
        disconnect(discHandler);
        
        return connected;
    }
    
    bool ensureConnected()
    {
        if (m_client->isConnected()) {
            return true;
        }
        
        const int maxAttempts = 3;
        const int baseTimeout = 1000; // 1 second
        
        for (int attempt = 0; attempt < maxAttempts; attempt++) {
            debugLog(QString("CDP connection attempt %1/%2").arg(attempt + 1).arg(maxAttempts));
            
            // Check if already connecting
            auto state = m_client->getConnectionState();
            if (state == CDPClient::ConnectionState::Connecting) {
                debugLog("Connection already in progress, waiting...");
                // If already connecting, just wait longer
                int timeout = baseTimeout * (1 << attempt);
                if (waitForConnection(timeout)) {
                    debugLog("CDP connection successful");
                    return true;
                }
            } else if (state == CDPClient::ConnectionState::NotConnected || 
                       state == CDPClient::ConnectionState::Failed) {
                // Start new connection attempt
                m_client->connect();
                
                // Exponential backoff: 1s, 2s, 4s
                int timeout = baseTimeout * (1 << attempt);
                
                if (waitForConnection(timeout)) {
                    debugLog("CDP connection successful");
                    return true;
                }
            }
            
            // If not last attempt, wait before retrying
            if (attempt < maxAttempts - 1) {
                int waitTime = baseTimeout * (1 << attempt) / 2;
                debugLog(QString("Connection failed, waiting %1ms before retry").arg(waitTime));
                QThread::msleep(waitTime);
            }
        }
        
        return false;
    }
    
    QJsonObject executeCommand(std::function<void(CDPClient*, CDPClient::ResponseCallback)> command)
    {
        const int maxRetries = 2;
        
        for (int retry = 0; retry <= maxRetries; retry++) {
            try {
                if (!ensureConnected()) {
                    debugLog("CDP connection failed after retries");
                    return createErrorResult(QString("Chrome DevTools not responding after multiple attempts. Make sure Tau5 is running in dev mode with --remote-debugging-port=%1").arg(m_client->getDevToolsPort()));
                }
                
                QEventLoop loop;
                QTimer timeout;
                timeout.setSingleShot(true);
                timeout.setInterval(5000);
                
                QJsonObject result;
                QString error;
                bool completed = false;
                
                connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
                
                command(m_client, [&](const QJsonObject& cdpResult, const QString& cdpError) {
                    result = cdpResult;
                    error = cdpError;
                    completed = true;
                    loop.quit();
                });
                
                timeout.start();
                loop.exec();
                
                if (!completed) {
                    debugLog("Command timeout");
                    // Check if this is a connection issue
                    if (!m_client->isConnected() && retry < maxRetries) {
                        debugLog("Connection lost, retrying command...");
                        QThread::msleep(1000);
                        continue;
                    }
                    return createErrorResult("CDP command timed out");
                }
                
                if (!error.isEmpty()) {
                    // Check if this is a connection-related error and we can retry
                    if ((error.contains("Not connected") || error.contains("Connection lost")) && retry < maxRetries) {
                        debugLog(QString("Connection error, retrying command: %1").arg(error));
                        QThread::msleep(1000);
                        continue;
                    }
                    debugLog(QString("Command error: %1").arg(error));
                    return createErrorResult(error);
                }
                
                return result;
            } catch (const std::exception& e) {
                if (retry < maxRetries) {
                    debugLog(QString("Exception caught, retrying: %1").arg(e.what()));
                    QThread::msleep(1000);
                    continue;
                }
                return createErrorResult(QString("Exception: %1").arg(e.what()));
            } catch (...) {
                if (retry < maxRetries) {
                    debugLog("Unknown exception caught, retrying...");
                    QThread::msleep(1000);
                    continue;
                }
                return createErrorResult("Unknown exception occurred");
            }
        }
        
        return createErrorResult("Failed after all retries");
    }
    
    QJsonObject formatResponse(const QJsonValue& data, bool returnRawJson = false)
    {
        if (returnRawJson) {
            // Return the raw JSON data directly
            if (data.isObject()) {
                return data.toObject();
            } else if (data.isArray()) {
                return QJsonObject{{"data", data}};
            } else {
                // For primitive values, wrap in an object
                return QJsonObject{{"value", data}};
            }
        } else {
            // Return as formatted text (existing behavior)
            QString text;
            if (data.isString()) {
                text = data.toString();
            } else if (data.isDouble()) {
                text = QString::number(data.toDouble());
            } else if (data.isBool()) {
                text = data.toBool() ? "true" : "false";
            } else if (data.isNull()) {
                text = "null";
            } else {
                // For objects and arrays, return pretty-printed JSON
                QJsonDocument doc(data.isArray() ? QJsonDocument(data.toArray()) : QJsonDocument(data.toObject()));
                text = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
            }
            return QJsonObject{
                {"type", "text"},
                {"text", text}
            };
        }
    }
    
private:
    QJsonObject createErrorResult(const QString& error)
    {
        return QJsonObject{
            {"type", "text"},
            {"text", QString("Error: %1").arg(error)}
        };
    }
    
    CDPClient* m_client;
};

#include "tau5_spectra.moc"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("tau5-spectra");
    app.setOrganizationName("Tau5");

    int channel = 0; // Default channel
    quint16 devToolsPort = 0; // 0 means not explicitly set
    bool debugMode = false;

    for (int i = 1; i < argc; i++) {
        QString arg = QString::fromUtf8(argv[i]);
        if (arg == "--channel" && i + 1 < argc) {
            channel = QString::fromUtf8(argv[++i]).toInt();
            if (channel < 0 || channel > 9) {
                std::cerr << "Error: --channel must be between 0 and 9\n";
                return 1;
            }
        } else if (arg == "--port-chrome-dev" && i + 1 < argc) {
            devToolsPort = QString::fromUtf8(argv[++i]).toUInt();
        } else if (arg == "--debug") {
            debugMode = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Tau5 Spectra\n\n";
            std::cout << "This server provides MCP (Model Context Protocol) access to Chrome DevTools.\n";
            std::cout << "It connects to a running Tau5 instance with DevTools enabled.\n\n";
            std::cout << "Usage: tau5-spectra [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --channel <0-9>         Channel number (0-9, default: 0)\n";
            std::cout << "                          Modifies default port: Chrome=922X\n";
            std::cout << "  --port-chrome-dev <n>   Chrome DevTools port (overrides channel default)\n";
            std::cout << "  --debug                 Enable debug logging to tau5-spectra-debug.log\n";
            std::cout << "  --help, -h              Show this help message\n\n";
            std::cout << "Configure in Claude Code with:\n";
            std::cout << "  \"mcpServers\": {\n";
            std::cout << "    \"tau5-spectra\": {\n";
            std::cout << "      \"command\": \"path/to/tau5-spectra\",\n";
            std::cout << "      \"args\": [\"--channel\", \"0\"]\n";
            std::cout << "    }\n";
            std::cout << "  }\n";
            return 0;
        }
    }

    // Apply channel-based default if port not explicitly set
    if (devToolsPort == 0) {
        devToolsPort = 9220 + channel;
    }

    // Calculate Tidewave port based on channel
    quint16 tidewavePort = 5550 + channel;

    // Don't use Tau5Logger for spectra - it needs a fixed log location
    // Initialize activity logger for Chromium DevTools only
    MCPActivityLogger chromiumLogger(QString("spectra-chromium-devtools-%1").arg(devToolsPort));

    // Note: Only chromium_devtools_* tools are logged
    // Tidewave and logs tools do not have logging to reduce noise

    debugLog("Tau5 GUI Dev MCP Server v1.0.0");
    debugLog(QString("Connecting to Chrome DevTools on port %1").arg(devToolsPort));
    debugLog(QString("Connecting to Tidewave MCP on port %1").arg(tidewavePort));

    MCPServerStdio server;
    server.setServerInfo("Tau5 GUI Dev MCP", "1.0.0");
    server.setCapabilities(QJsonObject{
        {"tools", QJsonObject{}}
    });
    server.setDebugMode(debugMode);

    auto cdpClient = std::make_unique<CDPClient>(devToolsPort);
    auto tidewaveProxy = std::make_unique<TidewaveProxy>(tidewavePort);

    CDPBridge bridge(cdpClient.get());
    TidewaveBridge tidewaveBridge(tidewaveProxy.get());

    // Initialize Tidewave proxy
    tidewaveProxy->checkAvailability();
    {
        QEventLoop initLoop;
        tidewaveProxy->initialize({}, [&initLoop](const QJsonObject& result, const QString& error) {
            if (error.isEmpty()) {
                debugLog("Tidewave MCP initialized successfully");
            } else {
                debugLog(QString("Tidewave unavailable: %1").arg(error));
            }
            initLoop.quit();
        });
        QTimer::singleShot(1000, &initLoop, &QEventLoop::quit);
        initLoop.exec();
    }
    
    QObject::connect(cdpClient.get(), &CDPClient::disconnected, [&cdpClient]() {
        debugLog("CDP Client disconnected - Tau5 may not be running");
    });
    
    QObject::connect(cdpClient.get(), &CDPClient::connectionFailed, [](const QString& error) {
        debugLog(QString("CDP connection error: %1").arg(error));
    });
    
    server.registerTool({
        "spectra_get_config",
        "Get Spectra's current configuration including channel, ports, and connection status",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [channel, devToolsPort, tidewavePort, &cdpClient, &tidewaveProxy](const QJsonObject&) -> QJsonObject {
            QJsonObject config;
            config["channel"] = channel;
            config["devToolsPort"] = devToolsPort;
            config["tidewavePort"] = tidewavePort;
            config["cdpConnected"] = cdpClient->isConnected();
            config["tidewaveAvailable"] = tidewaveProxy->isAvailable();

            QString configText = QString(
                "Spectra Configuration:\n"
                "  Channel: %1\n"
                "  Chrome DevTools Port: %2 (connected: %3)\n"
                "  Tidewave MCP Port: %4 (available: %5)"
            ).arg(channel)
             .arg(devToolsPort)
             .arg(cdpClient->isConnected() ? "yes" : "no")
             .arg(tidewavePort)
             .arg(tidewaveProxy->isAvailable() ? "yes" : "no");

            return QJsonObject{
                {"type", "text"},
                {"text", configText},
                {"data", config}
            };
        }
    });

    server.registerTool({
        "spectra_list_targets",
        "List all available Chrome DevTools targets",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&cdpClient](const QJsonObject&) -> QJsonObject {
            QJsonArray targets = cdpClient->getAvailableTargets();

            // Format targets for display
            QString output = "Available Chrome DevTools Targets:\n\n";
            int index = 0;
            for (const QJsonValue& value : targets) {
                QJsonObject target = value.toObject();
                QString type = target["type"].toString();
                QString title = target["title"].toString();
                QString url = target["url"].toString();

                if (type == "page") {
                    output += QString("%1. %2\n   URL: %3\n\n")
                        .arg(++index)
                        .arg(title.isEmpty() ? "(No title)" : title)
                        .arg(url);
                }
            }

            if (index == 0) {
                output = "No Chrome DevTools targets found. Make sure Tau5 is running.";
            } else {
                output += QString("Current target: %1").arg(cdpClient->getCurrentTargetTitle());
            }

            return QJsonObject{
                {"type", "text"},
                {"text", output},
                {"data", targets}
            };
        }
    });

    server.registerTool({
        "spectra_set_target",
        "Set the Chrome DevTools target by title",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"title", QJsonObject{
                    {"type", "string"},
                    {"description", "The title of the target to connect to (e.g., 'Tau5', 'Tau5 Console')"}
                }}
            }},
            {"required", QJsonArray{"title"}}
        },
        [&cdpClient](const QJsonObject& params) -> QJsonObject {
            QString title = params["title"].toString();

            if (title.isEmpty()) {
                return QJsonObject{
                    {"type", "text"},
                    {"text", "Error: Target title cannot be empty"}
                };
            }

            // Check if target exists in available targets
            QJsonArray targets = cdpClient->getAvailableTargets();
            bool targetFound = false;
            for (const QJsonValue& value : targets) {
                QJsonObject target = value.toObject();
                if (target["type"].toString() == "page" && target["title"].toString() == title) {
                    targetFound = true;
                    break;
                }
            }

            if (!targetFound) {
                return QJsonObject{
                    {"type", "text"},
                    {"text", QString("Error: No target found with title '%1'").arg(title)}
                };
            }

            bool success = cdpClient->setTargetByTitle(title);

            if (success) {
                return QJsonObject{
                    {"type", "text"},
                    {"text", QString("Successfully switched to target: %1").arg(title)}
                };
            } else {
                return QJsonObject{
                    {"type", "text"},
                    {"text", QString("Failed to switch to target: %1").arg(title)}
                };
            }
        }
    });

    server.registerTool({
        "chromium_devtools_getDocument",
        "Get the DOM document structure",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"depth", QJsonObject{
                    {"type", "integer"},
                    {"description", "Maximum depth to traverse (-1 for unlimited, default: 5)"}
                }}
            }}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            QJsonObject result = bridge.executeCommand([params](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getDocument(params, cb);
            });
            
            qint64 duration = timer.elapsed();
            
            if (result.contains("type") && result["type"].toString() == "text") {
                QString errorText = result["text"].toString();
                if (errorText.startsWith("Error: ")) {
                    chromiumLogger.logActivity("chromium_devtools_getDocument", requestId, params, "error", duration, errorText);
                } else {
                    QString truncatedResponse = errorText.left(500);
                    if (errorText.length() > 500) {
                        truncatedResponse += "... (truncated)";
                    }
                    chromiumLogger.logActivity("chromium_devtools_getDocument", requestId, params, "success", duration, QString(), truncatedResponse);
                }
                return result;
            }
            QJsonDocument doc(result);
            QString fullText = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
            QString truncatedResponse = fullText.left(500);
            if (fullText.length() > 500) {
                truncatedResponse += "... (truncated)";
            }
            chromiumLogger.logActivity("chromium_devtools_getDocument", requestId, params, "success", duration, QString(), truncatedResponse);
            
            QJsonObject resultObj;
            resultObj["type"] = "text";
            resultObj["text"] = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
            return resultObj;
        }
    });
    
    server.registerTool({
        "chromium_devtools_querySelector", 
        "Find elements matching a CSS selector",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"selector", QJsonObject{
                    {"type", "string"},
                    {"description", "CSS selector to match"}
                }}
            }},
            {"required", QJsonArray{"selector"}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();
            
            QString selector = params["selector"].toString();
            
            QJsonObject result = bridge.executeCommand([selector](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->querySelector(selector, cb);
            });
            
            qint64 duration = timer.elapsed();
            
            if (result.contains("type") && result["type"].toString() == "text") {
                QString errorText = result["text"].toString();
                chromiumLogger.logActivity("chromium_devtools_querySelector", requestId, params, "error", duration, errorText);
                return result;
            }
            
            int nodeId = result["nodeId"].toInt();
            if (nodeId == 0) {
                QString notFoundText = QString("No element found matching selector: %1").arg(selector);
                chromiumLogger.logActivity("chromium_devtools_querySelector", requestId, params, "not_found", duration, QString(), notFoundText);
                return QJsonObject{
                    {"type", "text"},
                    {"text", notFoundText}
                };
            }
            
            QString responseText = QString("Found element with nodeId: %1").arg(nodeId);
            chromiumLogger.logActivity("chromium_devtools_querySelector", requestId, params, "success", duration, QString(), responseText);
            
            return QJsonObject{
                {"type", "text"},
                {"text", responseText}
            };
        }
    });
    
    server.registerTool({
        "chromium_devtools_getOuterHTML",
        "Get the outer HTML of a DOM node",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"nodeId", QJsonObject{
                    {"type", "integer"},
                    {"description", "Node ID from querySelector or getDocument"}
                }}
            }},
            {"required", QJsonArray{"nodeId"}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();
            
            int nodeId = params["nodeId"].toInt();
            
            QJsonObject result = bridge.executeCommand([nodeId](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getOuterHTML(nodeId, cb);
            });
            
            qint64 duration = timer.elapsed();
            
            if (result.contains("type") && result["type"].toString() == "text") {
                QString errorText = result["text"].toString();
                chromiumLogger.logActivity("chromium_devtools_getOuterHTML", requestId, params, "error", duration, errorText);
                return result;
            }
            
            chromiumLogger.logActivity("chromium_devtools_getOuterHTML", requestId, params, "success", duration);
            
            return QJsonObject{
                {"type", "text"},
                {"text", result["outerHTML"].toString()}
            };
        }
    });
    
    server.registerTool({
        "chromium_devtools_evaluateJavaScript",
        "Execute JavaScript in the page context",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"expression", QJsonObject{
                    {"type", "string"},
                    {"description", "JavaScript expression to evaluate"}
                }}
            }},
            {"required", QJsonArray{"expression"}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();
            
            QString expression = params["expression"].toString();
            
            // First try with object references to avoid serialization issues
            QJsonObject result = bridge.executeCommand([expression](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->evaluateJavaScriptWithObjectReferences(expression, cb);
            });
            
            qint64 duration = timer.elapsed();
            
            if (result.contains("type") && result["type"].toString() == "text") {
                QString errorText = result["text"].toString();
                chromiumLogger.logActivity("chromium_devtools_evaluateJavaScript", requestId, params, "error", duration, errorText);
                return result;
            }
            
            if (result.contains("exceptionDetails")) {
                QJsonObject exception = result["exceptionDetails"].toObject();
                QString errorText = exception["text"].toString();
                chromiumLogger.logActivity("chromium_devtools_evaluateJavaScript", requestId, params, "exception", duration, errorText);
                return QJsonObject{
                    {"type", "text"},
                    {"text", QString("JavaScript exception: %1").arg(errorText)}
                };
            }
            
            QJsonObject resultObj = result["result"].toObject();
            
            // Check if we got an object reference instead of a value
            if (resultObj.contains("objectId") && !resultObj.contains("value")) {
                QString objectId = resultObj["objectId"].toString();
                QString className = resultObj["className"].toString();
                QString subtype = resultObj["subtype"].toString();
                QString type = resultObj["type"].toString();
                QString description = resultObj["description"].toString();
                
                QJsonObject objRef;
                objRef["type"] = "object_reference";
                objRef["objectId"] = objectId;
                objRef["className"] = className;
                objRef["objectType"] = type;
                objRef["subtype"] = subtype;
                objRef["description"] = description;
                
                // Return as JSON for complex objects
                QJsonDocument doc(objRef);
                QJsonObject responseObj;
                responseObj["type"] = "text";
                responseObj["text"] = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
                chromiumLogger.logActivity("chromium_devtools_evaluateJavaScript", requestId, params, "success", duration, QString(), objRef);
                return responseObj;
            }
            
            // Handle regular values (primitives)
            QJsonValue value = resultObj["value"];
            
            QString resultText;
            if (value.isString()) {
                resultText = value.toString();
            } else if (value.isDouble()) {
                resultText = QString::number(value.toDouble());
            } else if (value.isBool()) {
                resultText = value.toBool() ? "true" : "false";
            } else if (value.isObject() || value.isArray()) {
                QJsonDocument doc(value.isArray() ? QJsonDocument(value.toArray()) : QJsonDocument(value.toObject()));
                resultText = doc.toJson(QJsonDocument::Indented);
            } else if (value.isNull()) {
                resultText = "null";
            } else {
                resultText = "undefined";
            }
            
            chromiumLogger.logActivity("chromium_devtools_evaluateJavaScript", requestId, params, "success", duration, QString(), resultText);
            
            return QJsonObject{
                {"type", "text"},
                {"text", resultText}
            };
        }
    });

    server.registerTool({
        "chromium_devtools_hardRefresh",
        "Hard refresh the page by completely destroying and recreating the web view. This is much stronger than a normal refresh - it tears down the entire browser context and creates a new one from scratch. Essential for WASM/AudioWorklet development where modules can get stuck in memory, workers need to be fully terminated, or when SharedArrayBuffer/AudioContext state needs to be completely reset. Also useful when debugging memory leaks, testing initialization sequences, or when the browser cache is corrupted. Dev tools are automatically reconnected after the refresh (dev builds only)",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}},
            {"required", QJsonArray{}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            // Execute the hard refresh via JavaScript
            QString jsExpression = "window.tau5 && window.tau5.hardRefresh ? window.tau5.hardRefresh() : 'tau5.hardRefresh() not available (dev mode only)'";

            QJsonObject result = bridge.executeCommand([jsExpression](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->evaluateJavaScript(jsExpression, cb);
            });

            qint64 duration = timer.elapsed();

            if (result.contains("type") && result["type"].toString() == "text") {
                QString resultText = result["text"].toString();

                // Check if it's an error message about dev mode
                if (resultText.contains("not available")) {
                    chromiumLogger.logActivity("chromium_devtools_hardRefresh", requestId, params, "error", duration, resultText);
                    return QJsonObject{
                        {"type", "text"},
                        {"text", resultText}
                    };
                }

                chromiumLogger.logActivity("chromium_devtools_hardRefresh", requestId, params, "success", duration);
                return QJsonObject{
                    {"type", "text"},
                    {"text", "Hard refresh initiated"}
                };
            }

            chromiumLogger.logActivity("chromium_devtools_hardRefresh", requestId, params, "error", duration, "Unexpected response");
            return QJsonObject{
                {"type", "text"},
                {"text", "Failed to execute hard refresh"}
            };
        }
    });

    server.registerTool({
        "chromium_devtools_setAttribute",
        "Set an attribute on a DOM element",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"nodeId", QJsonObject{
                    {"type", "integer"},
                    {"description", "Node ID"}
                }},
                {"name", QJsonObject{
                    {"type", "string"},
                    {"description", "Attribute name"}
                }},
                {"value", QJsonObject{
                    {"type", "string"},
                    {"description", "Attribute value"}
                }}
            }},
            {"required", QJsonArray{"nodeId", "name", "value"}}
        },
        [&bridge](const QJsonObject& params) -> QJsonObject {
            int nodeId = params["nodeId"].toInt();
            QString name = params["name"].toString();
            QString value = params["value"].toString();
            
            QJsonObject result = bridge.executeCommand([nodeId, name, value](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->setAttributeValue(nodeId, name, value, cb);
            });
            
            if (result.contains("type") && result["type"].toString() == "text") {
                return result;
            }
            
            return QJsonObject{
                {"type", "text"},
                {"text", QString("Set attribute '%1' = '%2' on node %3").arg(name, value).arg(nodeId)}
            };
        }
    });
    
    server.registerTool({
        "chromium_devtools_removeAttribute",
        "Remove an attribute from a DOM element",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"nodeId", QJsonObject{
                    {"type", "integer"},
                    {"description", "Node ID"}
                }},
                {"name", QJsonObject{
                    {"type", "string"},
                    {"description", "Attribute name to remove"}
                }}
            }},
            {"required", QJsonArray{"nodeId", "name"}}
        },
        [&bridge](const QJsonObject& params) -> QJsonObject {
            int nodeId = params["nodeId"].toInt();
            QString name = params["name"].toString();
            
            QJsonObject result = bridge.executeCommand([nodeId, name](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->removeAttribute(nodeId, name, cb);
            });
            
            if (result.contains("type") && result["type"].toString() == "text") {
                return result;
            }
            
            return QJsonObject{
                {"type", "text"},
                {"text", QString("Removed attribute '%1' from node %2").arg(name).arg(nodeId)}
            };
        }
    });
    
    server.registerTool({
        "chromium_devtools_navigate",
        "Navigate within Tau5 app - use relative URLs like '/' or '/page'",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"url", QJsonObject{
                    {"type", "string"},
                    {"description", "Path to navigate to. Use relative URLs: '/' for home, '/page' for pages, '../' to go up. DO NOT use absolute URLs for normal navigation. Spectra handles ports automatically. /dev/* paths are blocked. (Advanced: External URLs like https://example.com work ONLY with --local-only=false for testing, NOT for regular app navigation)"}
                }}
            }},
            {"required", QJsonArray{"url"}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QElapsedTimer timer;
            timer.start();
            QString requestId = QUuid::createUuid().toString();

            QString url = params["url"].toString();

            QJsonObject result = bridge.executeCommand([url](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->navigateTo(url, cb);
            });

            qint64 duration = timer.elapsed();

            if (result.contains("type") && result["type"].toString() == "text") {
                QString errorText = result["text"].toString();
                chromiumLogger.logActivity("chromium_devtools_navigate", requestId, params, "error", duration, errorText);
                return result;
            }

            QString responseText = QString("Navigated to: %1").arg(url);
            chromiumLogger.logActivity("chromium_devtools_navigate", requestId, params, "success", duration, QString(), responseText);

            return QJsonObject{
                {"type", "text"},
                {"text", responseText}
            };
        }
    });
    
    server.registerTool({
        "chromium_devtools_getComputedStyle",
        "Get computed styles for an element",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"selector", QJsonObject{
                    {"type", "string"},
                    {"description", "CSS selector for the element"}
                }},
                {"properties", QJsonObject{
                    {"type", "array"},
                    {"description", "Optional array of specific CSS properties to retrieve (e.g., ['color', 'font-size']). If not specified, returns all properties."},
                    {"items", QJsonObject{{"type", "string"}}}
                }},
                {"rawJson", QJsonObject{
                    {"type", "boolean"},
                    {"description", "Return raw JSON instead of formatted text (default: false)"}
                }}
            }},
            {"required", QJsonArray{"selector"}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QElapsedTimer timer;
            timer.start();
            QString requestId = QUuid::createUuid().toString();

            QString selector = params["selector"].toString();
            QJsonArray requestedProps = params.contains("properties") ? params["properties"].toArray() : QJsonArray();
            bool rawJson = params.contains("rawJson") ? params["rawJson"].toBool() : false;
            
            QString propsArrayStr = "null";
            if (!requestedProps.isEmpty()) {
                QStringList propsList;
                for (const auto& prop : requestedProps) {
                    propsList.append(QString("'%1'").arg(prop.toString().replace("'", "\\'")));
                }
                propsArrayStr = QString("[%1]").arg(propsList.join(","));
            }
            
            QString jsExpression = QString(R"(
                (function() {
                    const element = document.querySelector('%1');
                    if (!element) return { error: 'Element not found' };
                    const styles = window.getComputedStyle(element);
                    const result = {};
                    const requestedProps = %2;
                    
                    if (requestedProps && requestedProps.length > 0) {
                        // Return only requested properties
                        for (const prop of requestedProps) {
                            result[prop] = styles.getPropertyValue(prop);
                        }
                    } else {
                        // Return all properties
                        for (let i = 0; i < styles.length; i++) {
                            const prop = styles[i];
                            result[prop] = styles.getPropertyValue(prop);
                        }
                    }
                    return result;
                })()
            )").arg(selector.replace("'", "\\'"), propsArrayStr);
            
            QJsonObject result = bridge.executeCommand([jsExpression](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->evaluateJavaScript(jsExpression, cb);
            });
            
            qint64 duration = timer.elapsed();

            if (result.contains("type") && result["type"].toString() == "text") {
                QString errorText = result["text"].toString();
                chromiumLogger.logActivity("chromium_devtools_getComputedStyle", requestId, params, "error", duration, errorText);
                return result;
            }

            QJsonObject resultObj = result["result"].toObject();
            QJsonValue value = resultObj["value"];

            if (value.isObject() && value.toObject().contains("error")) {
                QString errorText = value.toObject()["error"].toString();
                chromiumLogger.logActivity("chromium_devtools_getComputedStyle", requestId, params, "error", duration, errorText);
                if (rawJson) {
                    return QJsonObject{{"error", errorText}};
                }
                return QJsonObject{
                    {"type", "text"},
                    {"text", errorText}
                };
            }

            QJsonObject response = bridge.formatResponse(value, rawJson);
            QString responseText = response.contains("text") ? response["text"].toString().left(500) : "Computed styles retrieved";
            chromiumLogger.logActivity("chromium_devtools_getComputedStyle", requestId, params, "success", duration, QString(), responseText);
            return response;
        }
    });
    
    server.registerTool({
        "chromium_devtools_getProperties",
        "Get properties of a remote object",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"objectId", QJsonObject{
                    {"type", "string"},
                    {"description", "Remote object ID"}
                }}
            }},
            {"required", QJsonArray{"objectId"}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();
            
            QString objectId = params["objectId"].toString();
            
            QJsonObject result = bridge.executeCommand([objectId](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getProperties(objectId, cb);
            });
            
            qint64 duration = timer.elapsed();
            
            if (result.contains("type") && result["type"].toString() == "text") {
                QString errorText = result["text"].toString();
                chromiumLogger.logActivity("chromium_devtools_getProperties", requestId, params, "error", duration, errorText);
                return result;
            }
            
            if (result.contains("exceptionDetails")) {
                QJsonObject exception = result["exceptionDetails"].toObject();
                QString errorText = exception["text"].toString();
                chromiumLogger.logActivity("chromium_devtools_getProperties", requestId, params, "exception", duration, errorText);
                return QJsonObject{
                    {"type", "text"},
                    {"text", QString("Error: %1").arg(errorText)}
                };
            }
            
            // Format the properties for display
            QJsonArray properties = result["result"].toArray();
            QJsonObject formattedProps;
            
            for (const auto& prop : properties) {
                QJsonObject propObj = prop.toObject();
                QString name = propObj["name"].toString();
                QJsonObject value = propObj["value"].toObject();
                
                QJsonObject propInfo;
                propInfo["type"] = value["type"].toString();
                propInfo["value"] = value.contains("value") ? value["value"] : QJsonValue();
                propInfo["description"] = value["description"].toString();
                propInfo["className"] = value["className"].toString();
                formattedProps[name] = propInfo;
            }
            
            chromiumLogger.logActivity("chromium_devtools_getProperties", requestId, params, "success", duration, QString(), formattedProps);
            
            QJsonDocument doc(formattedProps);
            QJsonObject responseObj;
            responseObj["type"] = "text";
            responseObj["text"] = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
            return responseObj;
        }
    });
    
    server.registerTool({
        "chromium_devtools_callMethod",
        "Call a method on a remote object",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"objectId", QJsonObject{
                    {"type", "string"},
                    {"description", "Remote object ID"}
                }},
                {"functionDeclaration", QJsonObject{
                    {"type", "string"},
                    {"description", "Function to call on the object (e.g., 'function() { return this.textContent; }')"}
                }}
            }},
            {"required", QJsonArray{"objectId", "functionDeclaration"}}
        },
        [&bridge](const QJsonObject& params) -> QJsonObject {
            QString objectId = params["objectId"].toString();
            QString functionDecl = params["functionDeclaration"].toString();
            
            QJsonObject result = bridge.executeCommand([objectId, functionDecl](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->callFunctionOn(objectId, functionDecl, cb);
            });
            
            if (result.contains("type") && result["type"].toString() == "text") {
                return result;
            }
            
            if (result.contains("exceptionDetails")) {
                QJsonObject exception = result["exceptionDetails"].toObject();
                QString errorText = exception["text"].toString();
                return QJsonObject{
                    {"type", "text"},
                    {"text", QString("Error: %1").arg(errorText)}
                };
            }
            
            // Handle the result
            QJsonObject resultObj = result["result"].toObject();
            
            // Check if we got another object reference
            if (resultObj.contains("objectId") && !resultObj.contains("value")) {
                QJsonObject objRef;
                objRef["type"] = "object_reference";
                objRef["objectId"] = resultObj["objectId"].toString();
                objRef["className"] = resultObj["className"].toString();
                objRef["description"] = resultObj["description"].toString();
                
                QJsonDocument doc(objRef);
                QJsonObject responseObj;
                responseObj["type"] = "text";
                responseObj["text"] = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
                return responseObj;
            }
            
            // Handle regular values
            QJsonValue value = resultObj["value"];
            QString resultText;
            
            if (value.isString()) {
                resultText = value.toString();
            } else if (value.isDouble()) {
                resultText = QString::number(value.toDouble());
            } else if (value.isBool()) {
                resultText = value.toBool() ? "true" : "false";
            } else if (value.isObject() || value.isArray()) {
                QJsonDocument doc(value.isArray() ? QJsonDocument(value.toArray()) : QJsonDocument(value.toObject()));
                resultText = doc.toJson(QJsonDocument::Indented);
            } else if (value.isNull()) {
                resultText = "null";
            } else {
                resultText = "undefined";
            }
            
            return QJsonObject{
                {"type", "text"},
                {"text", resultText}
            };
        }
    });
    
    server.registerTool({
        "chromium_devtools_releaseObject",
        "Release a remote object reference",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"objectId", QJsonObject{
                    {"type", "string"},
                    {"description", "Remote object ID to release"}
                }}
            }},
            {"required", QJsonArray{"objectId"}}
        },
        [&bridge](const QJsonObject& params) -> QJsonObject {
            QString objectId = params["objectId"].toString();
            
            QJsonObject result = bridge.executeCommand([objectId](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->releaseObject(objectId, cb);
            });
            
            if (result.contains("type") && result["type"].toString() == "text") {
                return result;
            }
            
            return QJsonObject{
                {"type", "text"},
                {"text", QString("Released object: %1").arg(objectId)}
            };
        }
    });
    
    server.registerTool({
        "chromium_devtools_getSelectionInfo",
        "Get detailed information about the current text selection in the page, including DOM nodes, offsets, and context",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"includeContext", QJsonObject{
                    {"type", "boolean"},
                    {"description", "Include surrounding text context (default: true)"}
                }},
                {"contextLength", QJsonObject{
                    {"type", "integer"},
                    {"description", "Number of characters of context before/after selection (default: 50)"}
                }},
                {"includeStyles", QJsonObject{
                    {"type", "boolean"},
                    {"description", "Include computed styles for selected elements (default: false)"}
                }},
                {"includeHtml", QJsonObject{
                    {"type", "boolean"},
                    {"description", "Include outer HTML of affected elements (default: false)"}
                }},
                {"rawJson", QJsonObject{
                    {"type", "boolean"},
                    {"description", "Return raw JSON instead of formatted text (default: false)"}
                }}
            }},
            {"required", QJsonArray{}}
        },
        [&bridge](const QJsonObject& params) -> QJsonObject {
            bool includeContext = params.contains("includeContext") ? params["includeContext"].toBool() : true;
            int contextLength = params.contains("contextLength") ? params["contextLength"].toInt() : 50;
            bool includeStyles = params.contains("includeStyles") ? params["includeStyles"].toBool() : false;
            bool includeHtml = params.contains("includeHtml") ? params["includeHtml"].toBool() : false;
            bool rawJson = params.contains("rawJson") ? params["rawJson"].toBool() : false;
            
            QString jsExpression = QString(R"(
                (function() {
                    const selection = window.getSelection();
                    if (!selection || selection.rangeCount === 0) {
                        return { hasSelection: false };
                    }

                    const range = selection.getRangeAt(0);
                    const commonAncestor = range.commonAncestorContainer;
                    
                    // Helper to escape CSS identifiers
                    function escapeCSS(str) {
                        if (!str) return '';
                        // Based on CSS.escape polyfill
                        return str.replace(/([!"#$%&'()*+,.\/:;<=>?@[\\\]^`{|}~])/g, '\\\\$1');
                    }
                    
                    // Helper to build a unique selector for an element
                    function buildUniqueSelector(element) {
                        if (!element || element === document.documentElement) return 'html';
                        if (element === document.body) return 'body';
                        
                        // If element has an ID, use it (escaped)
                        if (element.id) {
                            return '#' + escapeCSS(element.id);
                        }
                        
                        // Build a path from the element to a parent with ID or body
                        const path = [];
                        let current = element;
                        
                        while (current && current !== document.body && current !== document.documentElement) {
                            let selector = current.tagName.toLowerCase();
                            
                            // Add classes if present
                            if (current.className && typeof current.className === 'string') {
                                const classes = current.className.trim().split(/\\s+/);
                                const escapedClasses = classes.map(cls => '.' + escapeCSS(cls)).join('');
                                selector += escapedClasses;
                            }
                            
                            // If we have an ID, we can stop here
                            if (current.id) {
                                selector = '#' + escapeCSS(current.id);
                                path.unshift(selector);
                                break;
                            }
                            
                            // Add nth-child if needed for uniqueness
                            if (current.parentElement) {
                                const siblings = Array.from(current.parentElement.children);
                                const sameTagSiblings = siblings.filter(s => s.tagName === current.tagName);
                                if (sameTagSiblings.length > 1) {
                                    const index = sameTagSiblings.indexOf(current) + 1;
                                    selector += ':nth-of-type(' + index + ')';
                                }
                            }
                            
                            path.unshift(selector);
                            current = current.parentElement;
                        }
                        
                        return path.join(' > ');
                    }
                    
                    // Helper to get node info
                    function getNodeInfo(node) {
                        const info = {
                            nodeType: node.nodeType,
                            nodeName: node.nodeName,
                            nodeValue: node.nodeValue,
                            isText: node.nodeType === Node.TEXT_NODE,
                            isElement: node.nodeType === Node.ELEMENT_NODE,
                            tagName: node.tagName ? node.tagName.toLowerCase() : null,
                            className: node.className || null,
                            id: node.id || null
                        };
                        
                        // Add path to node
                        if (node.nodeType === Node.ELEMENT_NODE) {
                            info.path = buildUniqueSelector(node);
                        } else if (node.parentElement) {
                            info.path = buildUniqueSelector(node.parentElement) + ' > #text';
                        } else {
                            info.path = '#text';
                        }
                        
                        return info;
                    }

                    // Get all nodes in the selection
                    const affectedNodes = [];
                    const treeWalker = document.createTreeWalker(
                        commonAncestor,
                        NodeFilter.SHOW_ALL,
                        {
                            acceptNode: function(node) {
                                if (selection.containsNode(node, true)) {
                                    return NodeFilter.FILTER_ACCEPT;
                                }
                                return NodeFilter.FILTER_SKIP;
                            }
                        }
                    );

                    let node;
                    while (node = treeWalker.nextNode()) {
                        const nodeInfo = getNodeInfo(node);
                        
                        // Check if this node is partially selected
                        if (node === range.startContainer || node === range.endContainer) {
                            nodeInfo.partial = true;
                            if (node === range.startContainer) {
                                nodeInfo.startOffset = range.startOffset;
                            }
                            if (node === range.endContainer) {
                                nodeInfo.endOffset = range.endOffset;
                            }
                        } else {
                            nodeInfo.partial = false;
                        }
                        
                        affectedNodes.push(nodeInfo);
                    }

                    // Get context if requested
                    let contextBefore = '';
                    let contextAfter = '';
                    if (%1) {
                        // Get text before selection
                        try {
                            const beforeRange = document.createRange();
                            beforeRange.setStart(commonAncestor, 0);
                            beforeRange.setEnd(range.startContainer, range.startOffset);
                            contextBefore = beforeRange.toString().slice(-%2);
                        } catch (e) {
                            // If commonAncestor is a text node, try its parent
                            try {
                                const parent = commonAncestor.parentNode;
                                const beforeRange = document.createRange();
                                beforeRange.setStart(parent, 0);
                                beforeRange.setEnd(range.startContainer, range.startOffset);
                                contextBefore = beforeRange.toString().slice(-%2);
                            } catch (e2) {
                                contextBefore = '';
                            }
                        }
                        
                        // Get text after selection
                        try {
                            const afterRange = document.createRange();
                            afterRange.setStart(range.endContainer, range.endOffset);
                            if (commonAncestor.nodeType === Node.TEXT_NODE) {
                                afterRange.setEnd(commonAncestor, commonAncestor.textContent.length);
                            } else {
                                afterRange.setEndAfter(commonAncestor.lastChild || commonAncestor);
                            }
                            contextAfter = afterRange.toString().slice(0, %2);
                        } catch (e) {
                            contextAfter = '';
                        }
                    }

                    // Get selection bounds
                    const rects = range.getClientRects();
                    const boundingRect = range.getBoundingClientRect();
                    
                    // Get element details if requested
                    let elementDetails = null;
                    if (%3 || %4) {
                        elementDetails = [];
                        const elements = new Set();
                        
                        // Collect unique element nodes
                        affectedNodes.forEach(nodeInfo => {
                            if (nodeInfo.isElement) {
                                // We'll need to query this separately since we can't pass DOM nodes
                                elementDetails.push({
                                    path: nodeInfo.path,
                                    tagName: nodeInfo.tagName,
                                    id: nodeInfo.id,
                                    className: nodeInfo.className
                                });
                            }
                        });
                        
                        // Also include parent elements of text nodes
                        if (range.startContainer.nodeType === Node.TEXT_NODE && range.startContainer.parentElement) {
                            const parent = getNodeInfo(range.startContainer.parentElement);
                            elementDetails.push({
                                path: parent.path,
                                tagName: parent.tagName,
                                id: parent.id,
                                className: parent.className,
                                isParentOfSelection: true
                            });
                        }
                    }

                    return {
                        hasSelection: true,
                        selectionText: selection.toString(),
                        isCollapsed: range.collapsed,
                        rangeCount: selection.rangeCount,
                        startContainer: getNodeInfo(range.startContainer),
                        startOffset: range.startOffset,
                        endContainer: getNodeInfo(range.endContainer),
                        endOffset: range.endOffset,
                        commonAncestor: getNodeInfo(commonAncestor),
                        affectedNodes: affectedNodes,
                        containsMultipleNodes: affectedNodes.length > 1,
                        contextBefore: contextBefore,
                        contextAfter: contextAfter,
                        bounds: {
                            top: boundingRect.top,
                            left: boundingRect.left,
                            bottom: boundingRect.bottom,
                            right: boundingRect.right,
                            width: boundingRect.width,
                            height: boundingRect.height
                        },
                        rectCount: rects.length,
                        elementDetails: elementDetails
                    };
                })()
            )").arg(includeContext ? "true" : "false")
               .arg(contextLength)
               .arg(includeStyles ? "true" : "false")
               .arg(includeHtml ? "true" : "false");
            
            QJsonObject result = bridge.executeCommand([jsExpression](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->evaluateJavaScript(jsExpression, cb);
            });
            
            if (result.contains("type") && result["type"].toString() == "text") {
                return result;
            }
            
            if (result.contains("exceptionDetails")) {
                QJsonObject exception = result["exceptionDetails"].toObject();
                QString errorText = exception["text"].toString();
                return QJsonObject{
                    {"type", "text"},
                    {"text", QString("JavaScript exception: %1").arg(errorText)}
                };
            }
            
            QJsonObject resultObj = result["result"].toObject();
            QJsonValue value = resultObj["value"];
            
            if (value.isObject()) {
                QJsonObject selectionInfo = value.toObject();
                
                if (!selectionInfo["hasSelection"].toBool()) {
                    return QJsonObject{
                        {"type", "text"},
                        {"text", "No text is currently selected"}
                    };
                }
                
                // If styles or HTML were requested, batch fetch them in a single request
                if ((includeStyles || includeHtml) && selectionInfo.contains("elementDetails")) {
                    QJsonArray elementDetails = selectionInfo["elementDetails"].toArray();
                    
                    // Build paths array for batch processing
                    QStringList pathsList;
                    for (const auto& elem : elementDetails) {
                        QJsonObject elemObj = elem.toObject();
                        QString path = elemObj["path"].toString();
                        if (!path.isEmpty() && !path.endsWith(" > #text")) {
                            pathsList.append(QString("'%1'").arg(path.replace("'", "\\'").replace("\\", "\\\\")));
                        } else {
                            pathsList.append("null");
                        }
                    }
                    
                    QString batchExpr = QString(R"(
                        (function() {
                            const paths = [%1];
                            const results = [];
                            
                            for (let i = 0; i < paths.length; i++) {
                                const path = paths[i];
                                const result = {};
                                
                                if (path) {
                                    const elem = document.querySelector(path);
                                    if (elem) {
                                        %2
                                        %3
                                    }
                                }
                                
                                results.push(result);
                            }
                            
                            return results;
                        })()
                    )").arg(pathsList.join(","))
                       .arg(includeStyles ? R"(
                                        const styles = window.getComputedStyle(elem);
                                        result.styles = {
                                            display: styles.display,
                                            position: styles.position,
                                            color: styles.color,
                                            backgroundColor: styles.backgroundColor,
                                            fontSize: styles.fontSize,
                                            fontWeight: styles.fontWeight,
                                            fontFamily: styles.fontFamily,
                                            lineHeight: styles.lineHeight,
                                            textAlign: styles.textAlign,
                                            padding: styles.padding,
                                            margin: styles.margin,
                                            border: styles.border
                                        };)" : "")
                       .arg(includeHtml ? "result.outerHtml = elem.outerHTML;" : "");
                    
                    QJsonObject batchResult = bridge.executeCommand([batchExpr](CDPClient* client, CDPClient::ResponseCallback cb) {
                        client->evaluateJavaScript(batchExpr, cb);
                    });
                    
                    if (!batchResult.contains("type") || batchResult["type"].toString() != "text") {
                        QJsonObject resultObj = batchResult["result"].toObject();
                        if (resultObj.contains("value") && resultObj["value"].isArray()) {
                            QJsonArray batchResults = resultObj["value"].toArray();
                            
                            // Merge batch results back into element details
                            for (int i = 0; i < elementDetails.size() && i < batchResults.size(); i++) {
                                QJsonObject elemObj = elementDetails[i].toObject();
                                QJsonObject batchItem = batchResults[i].toObject();
                                
                                if (batchItem.contains("styles")) {
                                    elemObj["styles"] = batchItem["styles"];
                                }
                                if (batchItem.contains("outerHtml")) {
                                    elemObj["outerHtml"] = batchItem["outerHtml"];
                                }
                                
                                elementDetails[i] = elemObj;
                            }
                            
                            selectionInfo["elementDetails"] = elementDetails;
                        }
                    }
                }
                
                // Return the selection info
                return bridge.formatResponse(selectionInfo, rawJson);
            }
            
            return QJsonObject{
                {"type", "text"},
                {"text", "Unexpected result format"}
            };
        }
    });
    
    server.registerTool({
        "tau5_logs_search",
        "Search Tau5 application logs on filesystem with regex patterns and filters - NOT browser console logs",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"sessions", QJsonObject{
                    {"type", "string"},
                    {"description", "Session selection: 'latest', 'all', or comma-separated indices like '0,1,2' (default: 'latest')"}
                }},
                {"pattern", QJsonObject{
                    {"type", "string"},
                    {"description", "Search pattern - can be plain text or regex (use with isRegex:true)"}
                }},
                {"isRegex", QJsonObject{
                    {"type", "boolean"},
                    {"description", "Treat pattern as regular expression (default: false)"}
                }},
                {"caseSensitive", QJsonObject{
                    {"type", "boolean"},
                    {"description", "Case-sensitive search (default: false)"}
                }},
                {"levels", QJsonObject{
                    {"type", "array"},
                    {"items", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"error", "warning", "info", "debug"}}}},
                    {"description", "Filter by log levels (empty = all levels)"}
                }},
                {"range", QJsonObject{
                    {"type", "object"},
                    {"properties", QJsonObject{
                        {"start", QJsonObject{{"type", "integer"}, {"description", "Starting line number (1-based)"}}},
                        {"end", QJsonObject{{"type", "integer"}, {"description", "Ending line number (inclusive)"}}},
                        {"last", QJsonObject{{"type", "integer"}, {"description", "Last N lines from end"}}}
                    }},
                    {"description", "Line range to search (omit for entire file)"}
                }},
                {"context", QJsonObject{
                    {"type", "integer"},
                    {"description", "Number of context lines before/after matches (default: 0)"}
                }},
                {"maxResults", QJsonObject{
                    {"type", "integer"},
                    {"description", "Maximum results to return per session (default: 100)"}
                }},
                {"format", QJsonObject{
                    {"type", "string"},
                    {"enum", QJsonArray{"full", "compact", "json"}},
                    {"description", "Output format: 'full' includes line numbers and session info, 'compact' is just matching lines, 'json' returns structured data (default: 'full')"}
                }}
            }},
            {"required", QJsonArray{}}
        },
        [&bridge, &chromiumLogger, channel](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            // Parse parameters
            QString sessions = params.value("sessions").toString("latest");
            QString pattern = params.value("pattern").toString();
            bool isRegex = params.value("isRegex").toBool(false);
            bool caseSensitive = params.value("caseSensitive").toBool(false);
            QJsonArray levelsArray = params.value("levels").toArray();
            QJsonObject range = params.value("range").toObject();
            int contextLines = params.value("context").toInt(0);
            int maxResults = params.value("maxResults").toInt(100);
            QString format = params.value("format").toString("full");
            
            // Convert levels array to set for fast lookup
            QSet<QString> levelFilter;
            for (const auto& level : levelsArray) {
                QString levelStr = level.toString().toUpper();
                if (levelStr == "WARNING") levelStr = "WARN";
                levelFilter.insert(QString("[%1]").arg(levelStr));
            }
            
            // Prepare regex if needed
            QRegularExpression regex;
            if (isRegex && !pattern.isEmpty()) {
                QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
                if (!caseSensitive) options |= QRegularExpression::CaseInsensitiveOption;
                regex = QRegularExpression(pattern, options);
                if (!regex.isValid()) {
                    return QJsonObject{
                        {"type", "text"},
                        {"text", QString("Invalid regex pattern: %1").arg(regex.errorString())}
                    };
                }
            }
            
            QString tau5DataPath = Tau5Logger::getTau5DataPath();
            QString tau5LogsPath = QDir(tau5DataPath).absoluteFilePath("logs/gui");
            
            // Get session directories and filter by channel
            QDir logsDir(tau5LogsPath);
            QStringList allSessionDirs = logsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

            // Filter sessions by channel suffix (e.g., "_c0", "_c1", etc.)
            QStringList sessionDirs;
            QString channelSuffix = QString("_c%1").arg(channel);
            for (const QString& dir : allSessionDirs) {
                if (dir.endsWith(channelSuffix)) {
                    sessionDirs.append(dir);
                }
            }

            if (sessionDirs.isEmpty()) {
                return QJsonObject{
                    {"type", "text"},
                    {"text", QString("No log sessions found for channel %1 in: %2").arg(channel).arg(tau5LogsPath)}
                };
            }
            
            // Determine which sessions to search
            QList<int> sessionIndices;
            if (sessions == "all") {
                for (int i = 0; i < sessionDirs.size(); ++i) {
                    sessionIndices.append(i);
                }
            } else if (sessions == "latest") {
                sessionIndices.append(0);
            } else {
                // Parse comma-separated indices
                QStringList indexStrs = sessions.split(',', Qt::SkipEmptyParts);
                for (const QString& indexStr : indexStrs) {
                    bool ok;
                    int idx = indexStr.trimmed().toInt(&ok);
                    if (ok && idx >= 0 && idx < sessionDirs.size()) {
                        sessionIndices.append(idx);
                    }
                }
            }
            
            // Process each session
            QJsonArray jsonResults;
            QStringList textResults;
            
            for (int sessionIdx : sessionIndices) {
                QString sessionName = sessionDirs.at(sessionIdx);
                QString sessionPath = QDir(tau5LogsPath).absoluteFilePath(sessionName);
                QString logFilePath = QDir(sessionPath).absoluteFilePath("gui.log");
                
                QFile logFile(logFilePath);
                if (!logFile.exists() || !logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    continue;
                }
                
                // Read file into memory with line numbers
                QTextStream stream(&logFile);
                QVector<QPair<int, QString>> lines;
                int lineNum = 1;
                while (!stream.atEnd()) {
                    lines.append(qMakePair(lineNum++, stream.readLine()));
                }
                logFile.close();
                
                // Apply range filter if specified
                if (!range.isEmpty()) {
                    if (range.contains("last")) {
                        int last = range["last"].toInt();
                        int start = qMax(0, lines.size() - last);
                        lines = lines.mid(start);
                    } else {
                        int start = range.value("start").toInt(1) - 1;
                        int end = range.value("end").toInt(lines.size());
                        start = qMax(0, start);
                        end = qMin(lines.size(), end);
                        lines = lines.mid(start, end - start);
                    }
                }
                
                // Search and filter (iterate in reverse for newest-first)
                QVector<QPair<int, QString>> matches;
                for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
                    const auto& [lineNum, line] = *it;
                    // Check level filter
                    if (!levelFilter.isEmpty()) {
                        bool hasLevel = false;
                        for (const QString& levelTag : levelFilter) {
                            if (line.contains(levelTag)) {
                                hasLevel = true;
                                break;
                            }
                        }
                        if (!hasLevel) continue;
                    }

                    // Check pattern
                    if (!pattern.isEmpty()) {
                        bool matched = false;
                        if (isRegex) {
                            matched = regex.match(line).hasMatch();
                        } else {
                            Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
                            matched = line.contains(pattern, cs);
                        }
                        if (!matched) continue;
                    }

                    matches.append(qMakePair(lineNum, line));
                    if (matches.size() >= maxResults) break;
                }
                
                // Format results
                if (format == "json") {
                    QJsonObject sessionResult;
                    sessionResult["session"] = sessionName;
                    sessionResult["file"] = logFilePath;
                    QJsonArray matchArray;
                    for (const auto& [lineNum, line] : matches) {
                        QJsonObject match;
                        match["line"] = lineNum;
                        match["text"] = line;
                        
                        // Add context if requested
                        if (contextLines > 0) {
                            QJsonArray beforeContext, afterContext;
                            int matchIdx = -1;
                            for (int i = 0; i < lines.size(); ++i) {
                                if (lines[i].first == lineNum) {
                                    matchIdx = i;
                                    break;
                                }
                            }
                            if (matchIdx >= 0) {
                                for (int i = qMax(0, matchIdx - contextLines); i < matchIdx; ++i) {
                                    beforeContext.append(lines[i].second);
                                }
                                for (int i = matchIdx + 1; i < qMin(lines.size(), matchIdx + contextLines + 1); ++i) {
                                    afterContext.append(lines[i].second);
                                }
                            }
                            if (!beforeContext.isEmpty()) match["before"] = beforeContext;
                            if (!afterContext.isEmpty()) match["after"] = afterContext;
                        }
                        matchArray.append(match);
                    }
                    sessionResult["matches"] = matchArray;
                    sessionResult["matchCount"] = matches.size();
                    jsonResults.append(sessionResult);
                } else {
                    // Text format
                    if (!matches.isEmpty()) {
                        if (format == "full") {
                            textResults.append(QString("\n=== Session: %1 ===").arg(sessionName));
                        }
                        for (const auto& [lineNum, line] : matches) {
                            if (format == "full") {
                                textResults.append(QString("[%1] %2").arg(lineNum, 6).arg(line));
                            } else {
                                textResults.append(line);
                            }
                        }
                    }
                }
            }
            
            qint64 duration = timer.elapsed();
            
            // Return results
            if (format == "json") {
                QJsonDocument doc(jsonResults);
                QString resultText = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
// Logging disabled for non-chromium tools:                 chromiumLogger.logActivity("tau5_logs_search", requestId, params, "success", duration, QString(), jsonResults);
                return QJsonObject{
                    {"type", "text"},
                    {"text", resultText}
                };
            } else {
                QString resultText = textResults.isEmpty() 
                    ? "No matches found" 
                    : textResults.join("\n");
// Logging disabled for non-chromium tools:                 chromiumLogger.logActivity("tau5_logs_search", requestId, params, "success", duration, QString(), resultText);
                return QJsonObject{
                    {"type", "text"},
                    {"text", resultText}
                };
            }
        }
    });
    
    server.registerTool({
        "tau5_logs_getSessions",
        "List all available Tau5 application log sessions with metadata - NOT browser console sessions",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}},
            {"required", QJsonArray{}}
        },
        [&chromiumLogger, channel](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();
            Q_UNUSED(params);
            
            QString tau5DataPath = Tau5Logger::getTau5DataPath();
            QString tau5LogsPath = QDir(tau5DataPath).absoluteFilePath("logs/gui");
            
            QDir logsDir(tau5LogsPath);
            QStringList allSessionDirs = logsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

            // Filter sessions by channel suffix (e.g., "_c0", "_c1", etc.)
            QStringList sessionDirs;
            QString channelSuffix = QString("_c%1").arg(channel);
            for (const QString& dir : allSessionDirs) {
                if (dir.endsWith(channelSuffix)) {
                    sessionDirs.append(dir);
                }
            }

            QJsonArray sessions;
            for (int i = 0; i < sessionDirs.size(); ++i) {
                QString sessionName = sessionDirs.at(i);
                QString sessionPath = QDir(tau5LogsPath).absoluteFilePath(sessionName);
                QString logFilePath = QDir(sessionPath).absoluteFilePath("gui.log");
                
                QJsonObject sessionInfo;
                sessionInfo["index"] = i;
                sessionInfo["name"] = sessionName;
                sessionInfo["path"] = logFilePath;
                
                QFile logFile(logFilePath);
                if (logFile.exists()) {
                    QFileInfo fileInfo(logFile);
                    sessionInfo["size"] = fileInfo.size();
                    sessionInfo["modified"] = fileInfo.lastModified().toString(Qt::ISODate);
                    
                    // Count lines
                    if (logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        int lines = 0;
                        QTextStream stream(&logFile);
                        while (!stream.atEnd()) {
                            stream.readLine();
                            lines++;
                        }
                        logFile.close();
                        sessionInfo["lines"] = lines;
                    }
                }
                
                sessions.append(sessionInfo);
            }
            
            QJsonDocument doc(sessions);
            QString resultText = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
            
            qint64 duration = timer.elapsed();
// Logging disabled for non-chromium tools:             chromiumLogger.logActivity("tau5_logs_getSessions", requestId, params, "success", duration, QString(), sessions);
            
            return QJsonObject{
                {"type", "text"},
                {"text", resultText}
            };
        }
    });
    
    server.registerTool({
        "tau5_logs_get",
        "Read Tau5 application logs from filesystem (beam, gui, mcp logs) - NOT browser console logs",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"lines", QJsonObject{
                    {"type", "integer"},
                    {"description", "Number of recent lines to return (default: 100)"}
                }},
                {"session", QJsonObject{
                    {"type", "integer"},
                    {"description", "Session index to read from (default: 0 for latest)"}
                }}
            }},
            {"required", QJsonArray{}}
        },
        [&chromiumLogger, channel](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();
            
            int numLines = params.value("lines").toInt(100);
            int sessionIdx = params.value("session").toInt(0);
            
            QString tau5DataPath = Tau5Logger::getTau5DataPath();
            QString tau5LogsPath = QDir(tau5DataPath).absoluteFilePath("logs/gui");

            QDir logsDir(tau5LogsPath);
            QStringList allSessionDirs = logsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

            // Filter sessions by channel suffix (e.g., "_c0", "_c1", etc.)
            QStringList sessionDirs;
            QString channelSuffix = QString("_c%1").arg(channel);
            for (const QString& dir : allSessionDirs) {
                if (dir.endsWith(channelSuffix)) {
                    sessionDirs.append(dir);
                }
            }

            if (sessionDirs.isEmpty() || sessionIdx >= sessionDirs.size()) {
                return QJsonObject{
                    {"type", "text"},
                    {"text", QString("No logs available for channel %1").arg(channel)}
                };
            }
            
            QString sessionPath = QDir(tau5LogsPath).absoluteFilePath(sessionDirs.at(sessionIdx));
            QString logFilePath = QDir(sessionPath).absoluteFilePath("gui.log");
            
            QFile logFile(logFilePath);
            if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                return QJsonObject{
                    {"type", "text"},
                    {"text", "Could not open log file"}
                };
            }
            
            // Read all lines
            QStringList allLines;
            QTextStream stream(&logFile);
            while (!stream.atEnd()) {
                allLines.append(stream.readLine());
            }
            logFile.close();
            
            // Get last N lines and reverse for newest-first order
            int startIdx = qMax(0, allLines.size() - numLines);
            QStringList resultLines = allLines.mid(startIdx);
            std::reverse(resultLines.begin(), resultLines.end());

            QString resultText = QString("Session: %1\n%2")
                .arg(sessionDirs.at(sessionIdx))
                .arg(resultLines.join("\n"));
            
            qint64 duration = timer.elapsed();
// Logging disabled for non-chromium tools:             chromiumLogger.logActivity("tau5_logs_get", requestId, params, "success", duration, QString(), resultText);
            
            return QJsonObject{
                {"type", "text"},
                {"text", resultText}
            };
        }
    });

    // Tool: Get JavaScript console messages with advanced filtering
    server.registerTool({
        "chromium_devtools_getConsoleMessages",
        "Get JavaScript console messages with filtering, search, and format options (default limit: 100)",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"limit", QJsonObject{
                    {"type", "integer"},
                    {"description", "Maximum number of messages to return (-1 for all, default: 100)"}
                }},
                {"level", QJsonObject{
                    {"oneOf", QJsonArray{
                        QJsonObject{{"type", "string"}},
                        QJsonObject{
                            {"type", "array"},
                            {"items", QJsonObject{{"type", "string"}}}
                        }
                    }},
                    {"description", "Filter by level(s): 'error', 'warn', 'log', 'info', 'debug'"}
                }},
                {"search", QJsonObject{
                    {"type", "string"},
                    {"description", "Search for text in messages (case-insensitive)"}
                }},
                {"regex", QJsonObject{
                    {"type", "string"},
                    {"description", "Filter messages with regex pattern"}
                }},
                {"since", QJsonObject{
                    {"type", "string"},
                    {"description", "ISO date to get messages after (e.g., '2025-01-01T10:30:00')"}
                }},
                {"last", QJsonObject{
                    {"type", "string"},
                    {"description", "Get messages from last period (e.g., '5m', '1h', '30s')"}
                }},
                {"since_last_call", QJsonObject{
                    {"type", "boolean"},
                    {"default", false},
                    {"description", "Only return messages since last getConsoleMessages call. Default: false. Automatically ignored when using search, regex, level, since, or last filters (searches always query full history). Use for streaming new messages only."}
                }},
                {"format", QJsonObject{
                    {"type", "string"},
                    {"enum", QJsonArray{"json", "plain", "csv"}},
                    {"description", "Output format (default: json)"}
                }}
            }}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            QJsonObject result = bridge.executeCommand([params](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getConsoleMessages(params, cb);
            });

            if (result.contains("type") && result["type"].toString() == "text") {
                QString errorText = result["text"].toString();
                if (errorText.startsWith("Error: ")) {
                    qint64 duration = timer.elapsed();
                    chromiumLogger.logActivity("chromium_devtools_getConsoleMessages", requestId, params, "error", duration, errorText);
                    return result;
                }
            }

            // Get format parameter and messages
            QString format = result.value("format").toString("json");
            QJsonArray messages = result["messages"].toArray();
            int count = result["count"].toInt();

            // Format output based on requested format
            QString output;

            if (format == "plain") {
                QStringList lines;
                for (const QJsonValue& val : messages) {
                    QJsonObject msg = val.toObject();
                    QString timestamp = msg["timestamp"].toString();
                    QString level = msg["level"].toString().toUpper();
                    QString text = msg["text"].toString();
                    QString location;

                    if (msg.contains("url") && msg.contains("lineNumber")) {
                        QString url = msg["url"].toString();
                        int line = msg["lineNumber"].toInt();
                        location = QString(" (%1:%2)").arg(url).arg(line);
                    }

                    lines.append(QString("[%1] [%2] %3%4")
                        .arg(timestamp)
                        .arg(level)
                        .arg(text)
                        .arg(location));

                    // Add stack trace if present
                    if (msg.contains("stackTrace")) {
                        lines.append(msg["stackTrace"].toString());
                    }
                }

                output = lines.join("\n");
                if (output.isEmpty()) {
                    output = "No console messages found";
                }
            } else if (format == "csv") {
                QStringList csvLines;
                csvLines.append("Timestamp,Level,Message,URL,Line,Column,Function");

                for (const QJsonValue& val : messages) {
                    QJsonObject msg = val.toObject();
                    QStringList fields;
                    fields << msg["timestamp"].toString();
                    fields << msg["level"].toString();
                    fields << QString("\"%1\"").arg(msg["text"].toString().replace("\"", "\\\""));
                    fields << msg.value("url").toString();
                    fields << QString::number(msg.value("lineNumber").toInt());
                    fields << QString::number(msg.value("columnNumber").toInt());
                    fields << msg.value("functionName").toString();
                    csvLines.append(fields.join(","));
                }

                output = csvLines.join("\n");
            } else {
                // JSON format - return structured data
                QJsonDocument doc(messages);
                output = QString("=== Console Messages (%1 total) ===\n").arg(count);
                output += QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
            }

            qint64 duration = timer.elapsed();
            chromiumLogger.logActivity("chromium_devtools_getConsoleMessages", requestId, params, "success", duration, QString(), output);

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Clear JavaScript console messages
    server.registerTool({
        "chromium_devtools_clearConsoleMessages",
        "Clear all stored JavaScript console messages",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->clearConsoleMessages();
                cb(QJsonObject{{"cleared", true}}, QString());
            });

            qint64 duration = timer.elapsed();
            chromiumLogger.logActivity("chromium_devtools_clearConsoleMessages", requestId, params, "success", duration);

            return QJsonObject{
                {"type", "text"},
                {"text", "Console messages cleared successfully"}
            };
        }
    });

    // Tool: Network Request Monitor
    server.registerTool({
        "chromium_devtools_getNetworkRequests",
        "Monitor network requests with WASM/AudioWorklet focus (default limit: 50)",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"urlPattern", QJsonObject{
                    {"type", "string"},
                    {"description", "Regex pattern to filter URLs (e.g., '.*\\.wasm|.*audioworklet.*')"}
                }},
                {"includeResponse", QJsonObject{
                    {"type", "boolean"},
                    {"description", "Include response details (status, headers, etc.)"}
                }},
                {"includeTimings", QJsonObject{
                    {"type", "boolean"},
                    {"description", "Include timing information"}
                }},
                {"limit", QJsonObject{
                    {"type", "integer"},
                    {"description", "Maximum number of requests to return (-1 for all, default: 100)"}
                }}
            }}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QElapsedTimer timer;
            timer.start();
            QString requestId = QUuid::createUuid().toString();
            QJsonObject result = bridge.executeCommand([params](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getNetworkRequests(params, cb);
            });

            if (result.contains("requests")) {
                QJsonArray requests = result["requests"].toArray();
                QString output = QString("=== Network Requests (%1 total) ===\n\n").arg(requests.size());

                for (const QJsonValue& val : requests) {
                    QJsonObject req = val.toObject();
                    output += QString("[%1] %2 %3\n")
                        .arg(req["timestamp"].toString())
                        .arg(req["method"].toString())
                        .arg(req["url"].toString());

                    if (req.contains("statusCode")) {
                        output += QString("  Status: %1 %2\n")
                            .arg(req["statusCode"].toInt())
                            .arg(req["statusText"].toString());
                    }

                    if (req.contains("failureReason")) {
                        output += QString("  FAILED: %1\n").arg(req["failureReason"].toString());
                    }

                    if (req.contains("responseHeaders")) {
                        QJsonObject headers = req["responseHeaders"].toObject();
                        if (headers.contains("cross-origin-opener-policy") ||
                            headers.contains("cross-origin-embedder-policy")) {
                            output += "  CORS Headers:\n";
                            if (headers.contains("cross-origin-opener-policy")) {
                                output += QString("    COOP: %1\n").arg(headers["cross-origin-opener-policy"].toString());
                            }
                            if (headers.contains("cross-origin-embedder-policy")) {
                                output += QString("    COEP: %1\n").arg(headers["cross-origin-embedder-policy"].toString());
                            }
                        }
                    }
                    output += "\n";
                }

                return QJsonObject{
                    {"type", "text"},
                    {"text", output}
                };
            }

            return result;
        }
    });

    // Tool: Get Memory Usage
    server.registerTool({
        "chromium_devtools_getMemoryUsage",
        "Get JavaScript heap and memory metrics",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject&) -> QJsonObject {
            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getMemoryUsage(cb);
            });

            QString output = "=== Memory Usage ===\n";
            for (auto it = result.begin(); it != result.end(); ++it) {
                output += QString("%1: %2\n").arg(it.key()).arg(it.value().toDouble());
            }

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Get Runtime Exceptions
    server.registerTool({
        "chromium_devtools_getExceptions",
        "Get uncaught exceptions and promise rejections",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject&) -> QJsonObject {
            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getPendingExceptions(cb);
            });

            if (result.contains("exceptions")) {
                QJsonArray exceptions = result["exceptions"].toArray();
                QString output = QString("=== Runtime Exceptions (%1 total) ===\n\n").arg(exceptions.size());

                for (const QJsonValue& val : exceptions) {
                    QJsonObject ex = val.toObject();
                    output += QString("[%1] %2\n")
                        .arg(ex["timestamp"].toString())
                        .arg(ex["text"].toString());
                    output += QString("  Location: %1:%2:%3\n")
                        .arg(ex["url"].toString())
                        .arg(ex["lineNumber"].toInt())
                        .arg(ex["columnNumber"].toInt());

                    if (ex.contains("stackTrace")) {
                        output += "  Stack Trace:\n";
                        QJsonDocument doc(ex["stackTrace"].toObject());
                        output += QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
                    }
                    output += "\n";
                }

                return QJsonObject{
                    {"type", "text"},
                    {"text", output}
                };
            }

            return result;
        }
    });

    // Tool: Get Loaded Resources
    server.registerTool({
        "chromium_devtools_getLoadedResources",
        "List all loaded page resources",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject&) -> QJsonObject {
            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getLoadedResources(cb);
            });

            if (result.contains("resources")) {
                QJsonArray resources = result["resources"].toArray();
                QString output = QString("=== Loaded Resources (%1 total) ===\n\n").arg(resources.size());

                QMap<QString, int> typeCount;
                for (const QJsonValue& val : resources) {
                    QJsonObject res = val.toObject();
                    QString type = res["type"].toString();
                    QString url = res["url"].toString();
                    typeCount[type]++;
                    output += QString("[%1] %2\n").arg(type).arg(url);
                }

                output += "\n=== Summary by Type ===\n";
                for (auto it = typeCount.begin(); it != typeCount.end(); ++it) {
                    output += QString("%1: %2\n").arg(it.key()).arg(it.value());
                }

                return QJsonObject{
                    {"type", "text"},
                    {"text", output}
                };
            }

            return result;
        }
    });

    // Tool: Get Audio Contexts
    server.registerTool({
        "chromium_devtools_getAudioContexts",
        "Get information about AudioContext instances",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject&) -> QJsonObject {
            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getAudioContexts(cb);
            });

            QString output = "=== Audio Contexts ===\n";
            if (result.contains("result")) {
                QJsonObject evalResult = result["result"].toObject();
                if (evalResult.contains("value")) {
                    QJsonArray contexts = evalResult["value"].toArray();
                    if (contexts.isEmpty()) {
                        output += "No AudioContext instances found\n";
                    } else {
                        for (const QJsonValue& val : contexts) {
                            QJsonObject ctx = val.toObject();
                            output += QString("State: %1\n").arg(ctx["state"].toString());
                            output += QString("Sample Rate: %1\n").arg(ctx["sampleRate"].toDouble());
                            output += QString("Current Time: %1\n").arg(ctx["currentTime"].toDouble());
                            output += QString("Base Latency: %1\n").arg(ctx["baseLatency"].toDouble());
                            output += QString("Output Latency: %1\n").arg(ctx["outputLatency"].toDouble());
                        }
                    }
                }
            }

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Get Workers
    server.registerTool({
        "chromium_devtools_getWorkers",
        "List active workers and worklets",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject&) -> QJsonObject {
            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getWorkers(cb);
            });

            if (result.contains("workers")) {
                QJsonArray workers = result["workers"].toArray();
                QString output = QString("=== Workers (%1 total) ===\n\n").arg(workers.size());

                for (const QJsonValue& val : workers) {
                    QJsonObject worker = val.toObject();
                    output += QString("[%1] %2\n")
                        .arg(worker["type"].toString())
                        .arg(worker["url"].toString());
                    output += QString("  Title: %1\n").arg(worker["title"].toString());
                    output += QString("  ID: %1\n\n").arg(worker["targetId"].toString());
                }

                return QJsonObject{
                    {"type", "text"},
                    {"text", output}
                };
            }

            return result;
        }
    });

    // Tool: Get Cross-Origin Isolation Status
    server.registerTool({
        "chromium_devtools_getCrossOriginIsolationStatus",
        "Check SharedArrayBuffer availability and COOP/COEP status",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QElapsedTimer timer;
            timer.start();
            QString requestId = QUuid::createUuid().toString();

            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getCrossOriginIsolationStatus(cb);
            });

            qint64 duration = timer.elapsed();

            QString output = "=== Cross-Origin Isolation Status ===\n";
            output += QString("SharedArrayBuffer Available: %1\n")
                .arg(result["sharedArrayBufferAvailable"].toBool() ? "YES" : "NO");
            output += QString("Cross-Origin Isolated: %1\n")
                .arg(result["crossOriginIsolated"].toBool() ? "YES" : "NO");
            output += QString("COEP Status: %1\n").arg(result["coep"].toString());
            output += QString("User Agent: %1\n").arg(result["userAgent"].toString());

            if (!result["crossOriginIsolated"].toBool()) {
                output += "\n⚠️ SharedArrayBuffer requires proper COOP/COEP headers:\n";
                output += "  - Cross-Origin-Opener-Policy: same-origin\n";
                output += "  - Cross-Origin-Embedder-Policy: require-corp\n";
            }

            chromiumLogger.logActivity("chromium_devtools_getCrossOriginIsolationStatus", requestId, params, "success", duration, QString(), "Cross-origin isolation status retrieved");

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Get Security State
    server.registerTool({
        "chromium_devtools_getSecurityState",
        "Get page security state and certificate info",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QElapsedTimer timer;
            timer.start();
            QString requestId = QUuid::createUuid().toString();

            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getSecurityState(cb);
            });

            qint64 duration = timer.elapsed();

            QString output = "=== Security State ===\n";
            output += QString("Security State: %1\n").arg(result["securityState"].toString());

            if (result.contains("certificateSecurityState")) {
                QJsonObject cert = result["certificateSecurityState"].toObject();
                output += QString("Certificate Valid: %1\n").arg(cert["certificateHasWeakSignature"].toBool() ? "NO" : "YES");
                output += QString("Protocol: %1\n").arg(cert["protocol"].toString());
            }

            chromiumLogger.logActivity("chromium_devtools_getSecurityState", requestId, params, "success", duration, QString(), "Security state retrieved");

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Monitor WASM Instantiation
    server.registerTool({
        "chromium_devtools_monitorWasmInstantiation",
        "Monitor WebAssembly module instantiation attempts",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject&) -> QJsonObject {
            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->monitorWasmInstantiation(cb);
            });

            QString output = "=== WASM Instantiation Monitor ===\n";

            if (result.contains("available") && !result["available"].toBool()) {
                output += "WebAssembly API not available\n";
            } else {
                output += QString("Monitoring Enabled: %1\n")
                    .arg(result["monitoringEnabled"].toBool() ? "YES" : "NO");

                if (result.contains("instantiations")) {
                    QJsonArray instantiations = result["instantiations"].toArray();
                    output += QString("\nInstantiation Attempts: %1\n\n").arg(instantiations.size());

                    for (const QJsonValue& val : instantiations) {
                        QJsonObject inst = val.toObject();
                        output += QString("[%1] Method: %2\n")
                            .arg(inst["timestamp"].toString())
                            .arg(inst["method"].toString());
                        output += QString("  Success: %1\n")
                            .arg(inst["success"].toBool() ? "YES" : "NO");

                        if (!inst["success"].toBool()) {
                            output += QString("  Error: %1\n").arg(inst["error"].toString());
                        } else {
                            if (inst.contains("exports")) {
                                QJsonArray exports = inst["exports"].toArray();
                                output += QString("  Exports: %1 functions\n").arg(exports.size());
                            }
                        }

                        output += QString("  Duration: %1ms\n\n").arg(inst["duration"].toDouble());
                    }
                }

                output += "\n📝 Console will show [WASM] prefixed messages for future instantiations\n";
            }

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Get AudioWorklet State
    server.registerTool({
        "chromium_devtools_getAudioWorkletState",
        "Check AudioWorklet availability and state",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject&) -> QJsonObject {
            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getAudioWorkletState(cb);
            });

            QString output = "=== AudioWorklet State ===\n";

            // Check AudioWorklet availability
            if (result.contains("audioWorkletNodeAvailable")) {
                output += QString("AudioWorkletNode API: %1\n")
                    .arg(result["audioWorkletNodeAvailable"].toBool() ? "Available" : "Not Available");
            }

            if (result.contains("audioWorkletAvailable")) {
                output += QString("AudioWorklet on Context: %1\n")
                    .arg(result["audioWorkletAvailable"].toBool() ? "Available" : "Not Available");
            }

            output += QString("SharedArrayBuffer: %1\n")
                .arg(result["sharedArrayBufferAvailable"].toBool() ? "Available" : "Not Available");

            // Audio contexts
            if (result.contains("audioContexts")) {
                QJsonArray contexts = result["audioContexts"].toArray();
                output += QString("\nAudio Contexts: %1\n").arg(contexts.size());

                for (const QJsonValue& val : contexts) {
                    QJsonObject ctx = val.toObject();
                    output += QString("\n  State: %1\n").arg(ctx["state"].toString());
                    output += QString("  Sample Rate: %1\n").arg(ctx["sampleRate"].toDouble());
                    output += QString("  Current Time: %1\n").arg(ctx["currentTime"].toDouble());
                    output += QString("  Has Worklet: %1\n")
                        .arg(ctx["hasWorklet"].toBool() ? "YES" : "NO");
                }
            }

            if (!result["audioWorkletAvailable"].toBool()) {
                output += "\n⚠️ AudioWorklet not available - needed for WASM audio processing\n";
            }

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Get Performance Timeline
    server.registerTool({
        "chromium_devtools_getPerformanceTimeline",
        "Get performance timeline for WASM/AudioWorklet resources",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QElapsedTimer timer;
            timer.start();
            QString requestId = QUuid::createUuid().toString();

            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getPerformanceTimeline(cb);
            });

            qint64 duration = timer.elapsed();

            QString output = "=== Performance Timeline ===\n";

            // Navigation timing
            if (result.contains("result")) {
                QJsonObject timeline = result["result"].toObject()["value"].toObject();

                if (timeline.contains("navigation")) {
                    QJsonObject nav = timeline["navigation"].toObject();
                    output += "\nNavigation Timing:\n";
                    output += QString("  DOM Content Loaded: %1ms\n").arg(nav["domContentLoaded"].toDouble());
                    output += QString("  Page Load Complete: %1ms\n").arg(nav["loadComplete"].toDouble());
                }

                // WASM and AudioWorklet resources
                if (timeline.contains("resources")) {
                    QJsonArray resources = timeline["resources"].toArray();
                    if (!resources.isEmpty()) {
                        output += "\nWASM/AudioWorklet Resources:\n";
                        for (const QJsonValue& val : resources) {
                            QJsonObject res = val.toObject();
                            output += QString("\n  %1\n").arg(res["name"].toString());
                            output += QString("    Duration: %1ms\n").arg(res["duration"].toDouble());
                            output += QString("    Start Time: %1ms\n").arg(res["startTime"].toDouble());
                            output += QString("    Transfer Size: %1 bytes\n").arg(res["transferSize"].toDouble());
                            output += QString("    Decoded Size: %1 bytes\n").arg(res["decodedBodySize"].toDouble());
                        }
                    } else {
                        output += "\nNo WASM or AudioWorklet resources found in timeline\n";
                    }
                }

                // Memory info
                if (timeline.contains("memory")) {
                    QJsonObject mem = timeline["memory"].toObject();
                    output += "\nMemory Usage:\n";
                    output += QString("  Used JS Heap: %1 MB\n").arg(mem["usedJSHeapSize"].toDouble() / 1048576);
                    output += QString("  Total JS Heap: %1 MB\n").arg(mem["totalJSHeapSize"].toDouble() / 1048576);
                }
            }

            chromiumLogger.logActivity("chromium_devtools_getPerformanceTimeline", requestId, params, "success", duration, QString(), "Performance timeline retrieved");

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Get Network Response Body
    server.registerTool({
        "chromium_devtools_getResponseBody",
        "Get response body for a network request (check if WASM actually loaded)",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"requestId", QJsonObject{
                    {"type", "string"},
                    {"description", "Request ID from getNetworkRequests"}
                }}
            }},
            {"required", QJsonArray{"requestId"}}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = params["requestId"].toString();

            QJsonObject result = bridge.executeCommand([requestId](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getResponseBody(requestId, cb);
            });

            QString output = "=== Response Body Info ===\n";
            output += QString("Request ID: %1\n").arg(requestId);

            if (result.contains("base64Encoded")) {
                output += QString("Base64 Encoded: %1\n").arg(result["base64Encoded"].toBool() ? "YES" : "NO");

                if (result.contains("decodedSize")) {
                    output += QString("Decoded Size: %1 bytes\n").arg(result["decodedSize"].toInt());
                }

                if (result.contains("isWasmModule") && result["isWasmModule"].toBool()) {
                    output += "\n✅ Valid WASM Module Detected!\n";
                    output += QString("WASM Version: %1\n").arg(result["wasmVersion"].toInt());
                } else if (result["base64Encoded"].toBool()) {
                    output += "\n❌ Not a valid WASM module (wrong magic number)\n";
                }

                // Show first 100 chars of body if not binary
                if (!result["base64Encoded"].toBool()) {
                    QString body = result["body"].toString();
                    if (body.length() > 100) {
                        output += QString("\nFirst 100 chars:\n%1...\n").arg(body.left(100));
                    } else {
                        output += QString("\nBody:\n%1\n").arg(body);
                    }
                }
            } else {
                output += "Unable to retrieve response body\n";
            }

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // LiveView debugging tools
    // Tool: Get WebSocket Frames
    server.registerTool({
        "chromium_devtools_getWebSocketFrames",
        "Get WebSocket frames for LiveView debugging (default limit: 100)",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"url", QJsonObject{
                    {"type", "string"},
                    {"description", "Filter by URL containing this string"}
                }},
                {"sentOnly", QJsonObject{
                    {"type", "boolean"},
                    {"description", "Show only sent frames"}
                }},
                {"receivedOnly", QJsonObject{
                    {"type", "boolean"},
                    {"description", "Show only received frames"}
                }},
                {"search", QJsonObject{
                    {"type", "string"},
                    {"description", "Search in frame payload data"}
                }},
                {"limit", QJsonObject{
                    {"type", "integer"},
                    {"description", "Maximum number of frames to return (-1 for all, default: 100)"}
                }}
            }}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QElapsedTimer timer;
            timer.start();
            QString requestId = QUuid::createUuid().toString();
            QJsonObject result = bridge.executeCommand([params](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getWebSocketFrames(params, cb);
            });

            QString output = "=== WebSocket Frames ===\n\n";
            QJsonArray frames = result["frames"].toArray();
            int total = result["total"].toInt();

            if (frames.isEmpty()) {
                output += "No WebSocket frames captured.\n";
            } else {
                for (const QJsonValue& val : frames) {
                    QJsonObject frame = val.toObject();
                    output += QString("[%1] %2 %3\n")
                        .arg(frame["timestamp"].toString())
                        .arg(frame["direction"].toString().toUpper())
                        .arg(frame["url"].toString());

                    if (frame.contains("liveViewEvent")) {
                        output += QString("  LiveView Event: %1\n").arg(frame["liveViewEvent"].toString());
                    }

                    if (frame.contains("parsedData")) {
                        QJsonDocument doc(frame["parsedData"].toObject());
                        if (doc.isEmpty()) {
                            doc = QJsonDocument(frame["parsedData"].toArray());
                        }
                        output += "  Parsed: " + doc.toJson(QJsonDocument::Compact) + "\n";
                    } else if (frame.contains("data")) {
                        QString data = frame["data"].toString();
                        if (data.length() > 200) {
                            data = data.left(200) + "...";
                        }
                        output += "  Data: " + data + "\n";
                    }
                    output += "\n";
                }
            }

            output += QString("\nTotal frames captured: %1\n").arg(total);

            qint64 duration = timer.elapsed();
            chromiumLogger.logActivity("chromium_devtools_getWebSocketFrames", requestId, params, "success", duration, QString(), QString("Retrieved %1 WebSocket frames").arg(frames.size()));

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Clear WebSocket Frames
    server.registerTool({
        "chromium_devtools_clearWebSocketFrames",
        "Clear captured WebSocket frames",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject&) -> QJsonObject {
            bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback) {
                client->clearWebSocketFrames();
            });

            return QJsonObject{
                {"type", "text"},
                {"text", "WebSocket frames cleared successfully"}
            };
        }
    });

    // Tool: Start DOM Mutation Observer
    server.registerTool({
        "chromium_devtools_startDOMMutationObserver",
        "Start observing DOM mutations for LiveView morphdom tracking",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"selector", QJsonObject{
                    {"type", "string"},
                    {"description", "CSS selector for element to observe (default: body)"}
                }}
            }}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString selector = params["selector"].toString();
            if (selector.isEmpty()) {
                selector = "body";
            }

            QJsonObject result = bridge.executeCommand([selector](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->startDOMMutationObserver(selector, cb);
            });

            QString output;
            if (result.contains("error")) {
                output = QString("Failed to start observer: %1").arg(result["error"].toString());
            } else if (result["success"].toBool()) {
                output = QString("DOM Mutation Observer started on: %1\n").arg(result["observing"].toString());
                output += "\nMutations will be captured in the console with [DOM_MUTATION] prefix.\n";
                output += "Use getDOMMutations to retrieve captured mutations.";
            } else {
                output = "Failed to start DOM Mutation Observer";
            }

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Stop DOM Mutation Observer
    server.registerTool({
        "chromium_devtools_stopDOMMutationObserver",
        "Stop observing DOM mutations",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject&) -> QJsonObject {
            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->stopDOMMutationObserver(cb);
            });

            QString output;
            if (result["success"].toBool()) {
                output = "DOM Mutation Observer stopped successfully";
            } else {
                output = QString("Failed to stop observer: %1").arg(result.value("error").toString("Unknown error"));
            }

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Get DOM Mutations
    server.registerTool({
        "chromium_devtools_getDOMMutations",
        "Get captured DOM mutations",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"limit", QJsonObject{
                    {"type", "integer"},
                    {"description", "Maximum number of mutations to return (-1 for all, default: 100)"}
                }}
            }}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QJsonObject result = bridge.executeCommand([params](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getDOMMutations(params, cb);
            });

            QString output = "=== DOM Mutations ===\n\n";
            QJsonArray mutations = result["mutations"].toArray();

            if (mutations.isEmpty()) {
                output += "No DOM mutations captured.\n";
                output += "Start observing with chromium_devtools_startDOMMutationObserver first.";
            } else {
                for (const QJsonValue& val : mutations) {
                    QJsonObject mutation = val.toObject();
                    output += QString("[%1] %2\n")
                        .arg(mutation["timestamp"].toString())
                        .arg(mutation["type"].toString());

                    if (mutation.contains("target")) {
                        output += QString("  Target: %1\n").arg(mutation["target"].toString());
                    }
                    if (mutation.contains("attributeName")) {
                        output += QString("  Attribute: %1\n").arg(mutation["attributeName"].toString());
                    }
                    if (mutation.contains("oldValue")) {
                        output += QString("  Old Value: %1\n").arg(mutation["oldValue"].toString());
                    }
                    if (mutation.contains("addedNodes")) {
                        QJsonArray added = mutation["addedNodes"].toArray();
                        if (!added.isEmpty()) {
                            output += "  Added: ";
                            for (const QJsonValue& node : added) {
                                output += node.toString() + " ";
                            }
                            output += "\n";
                        }
                    }
                    if (mutation.contains("removedNodes")) {
                        QJsonArray removed = mutation["removedNodes"].toArray();
                        if (!removed.isEmpty()) {
                            output += "  Removed: ";
                            for (const QJsonValue& node : removed) {
                                output += node.toString() + " ";
                            }
                            output += "\n";
                        }
                    }
                    output += "\n";
                }
            }

            output += QString("\nTotal mutations: %1\n").arg(mutations.size());

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tool: Clear DOM Mutations
    server.registerTool({
        "chromium_devtools_clearDOMMutations",
        "Clear captured DOM mutations",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject&) -> QJsonObject {
            bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback) {
                client->clearDOMMutations();
            });

            return QJsonObject{
                {"type", "text"},
                {"text", "DOM mutations cleared successfully"}
            };
        }
    });

    // Tool: Get JavaScript Performance Profile
    server.registerTool({
        "chromium_devtools_getJavaScriptProfile",
        "Get JavaScript performance metrics for LiveView hooks",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject&) -> QJsonObject {
            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getJavaScriptProfile(cb);
            });

            QString output = "=== JavaScript Performance Profile ===\n\n";

            // Performance measures
            QJsonArray measures = result["measures"].toArray();
            if (!measures.isEmpty()) {
                output += "Performance Measures:\n";
                for (const QJsonValue& val : measures) {
                    QJsonObject measure = val.toObject();
                    output += QString("  %1: %2ms (start: %3ms)\n")
                        .arg(measure["name"].toString())
                        .arg(measure["duration"].toDouble(), 0, 'f', 2)
                        .arg(measure["startTime"].toDouble(), 0, 'f', 2);
                }
                output += "\n";
            }

            // Hook stats
            QJsonObject hookStats = result["hookStats"].toObject();
            if (!hookStats.isEmpty()) {
                output += "LiveView Hook Stats:\n";
                for (auto it = hookStats.begin(); it != hookStats.end(); ++it) {
                    output += QString("  %1: %2\n").arg(it.key()).arg(it.value().toString());
                }
                output += "\n";
            }

            // Memory usage
            if (result.contains("usedJSHeapSize")) {
                double used = result["usedJSHeapSize"].toDouble() / 1024 / 1024;
                double total = result["totalJSHeapSize"].toDouble() / 1024 / 1024;
                output += QString("Memory Usage:\n");
                output += QString("  Used: %.2f MB\n").arg(used);
                output += QString("  Total: %.2f MB\n").arg(total);
                output += QString("  Usage: %.1f%%\n").arg((used / total) * 100);
            }

            if (measures.isEmpty() && hookStats.isEmpty()) {
                output += "No performance data captured.\n";
                output += "LiveView hooks can be profiled by adding performance.mark() calls.";
            }

            return QJsonObject{
                {"type", "text"},
                {"text", output}
            };
        }
    });

    // Tidewave MCP Proxy Tools - All tools exposed with tidewave_ prefix

    server.registerTool({
        "tidewave_get_logs",
        "Returns all log output from Tidewave, excluding logs that were caused by other tool calls. Use this tool to check for request logs or potentially logged errors.",
        QJsonObject{
            {"type", "object"},
            {"required", QJsonArray{"tail"}},
            {"properties", QJsonObject{
                {"tail", QJsonObject{
                    {"type", "number"},
                    {"description", "The number of log entries to return from the end of the log"}
                }}
            }}
        },
        [&tidewaveBridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            QJsonObject result = tidewaveBridge.executeCommand("get_logs", params);

            if (result.contains("error")) {
                QString error = result["message"].toString();
// Logging disabled for non-chromium tools:                 chromiumLogger.logActivity("tidewave_get_logs", requestId, params, "error", timer.elapsed(), error);
                return QJsonObject{
                    {"type", "text"},
                    {"text", error}
                };
            }

// Logging disabled for non-chromium tools:             chromiumLogger.logActivity("tidewave_get_logs", requestId, params, "success", timer.elapsed(), QString(), result);
            return tidewaveBridge.formatResponse(result);
        }
    });

    server.registerTool({
        "tidewave_get_source_location",
        "Returns the source location for the given reference. Works for modules in the current project and dependencies (but not Elixir itself). Use when you know the Module, Module.function, or Module.function/arity. You can also use 'dep:PACKAGE_NAME' to get the location of a specific dependency package.",
        QJsonObject{
            {"type", "object"},
            {"required", QJsonArray{"reference"}},
            {"properties", QJsonObject{
                {"reference", QJsonObject{
                    {"type", "string"},
                    {"description", "The reference to find (e.g., 'MyModule', 'MyModule.function', 'MyModule.function/2', or 'dep:package_name')"}
                }}
            }}
        },
        [&tidewaveBridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            QJsonObject result = tidewaveBridge.executeCommand("get_source_location", params);

            if (result.contains("error")) {
                QString error = result["message"].toString();
// Logging disabled for non-chromium tools:                 chromiumLogger.logActivity("tidewave_get_source_location", requestId, params, "error", timer.elapsed(), error);
                return QJsonObject{
                    {"type", "text"},
                    {"text", error}
                };
            }

// Logging disabled for non-chromium tools:             chromiumLogger.logActivity("tidewave_get_source_location", requestId, params, "success", timer.elapsed(), QString(), result);
            return tidewaveBridge.formatResponse(result);
        }
    });

    server.registerTool({
        "tidewave_get_docs",
        "Returns the documentation for the given reference (Module or Module.function)",
        QJsonObject{
            {"type", "object"},
            {"required", QJsonArray{"reference"}},
            {"properties", QJsonObject{
                {"reference", QJsonObject{
                    {"type", "string"},
                    {"description", "The reference to get docs for (e.g., 'MyModule' or 'MyModule.function')"}
                }}
            }}
        },
        [&tidewaveBridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            QJsonObject result = tidewaveBridge.executeCommand("get_docs", params);

            if (result.contains("error")) {
                QString error = result["message"].toString();
// Logging disabled for non-chromium tools:                 chromiumLogger.logActivity("tidewave_get_docs", requestId, params, "error", timer.elapsed(), error);
                return QJsonObject{
                    {"type", "text"},
                    {"text", error}
                };
            }

// Logging disabled for non-chromium tools:             chromiumLogger.logActivity("tidewave_get_docs", requestId, params, "success", timer.elapsed(), QString(), result);
            return tidewaveBridge.formatResponse(result);
        }
    });

    server.registerTool({
        "tidewave_project_eval",
        "Evaluates Elixir code in the context of the project. Use this tool every time you need to evaluate Elixir code, including to test the behaviour of a function or to debug something. The tool also returns anything written to standard output. DO NOT use shell tools to evaluate Elixir code. It also includes IEx helpers in the evaluation context.",
        QJsonObject{
            {"type", "object"},
            {"required", QJsonArray{"code"}},
            {"properties", QJsonObject{
                {"code", QJsonObject{
                    {"type", "string"},
                    {"description", "The Elixir code to evaluate"}
                }},
                {"arguments", QJsonObject{
                    {"type", "array"},
                    {"description", "The arguments to pass to evaluation. They are available inside the evaluated code as `arguments`"},
                    {"items", QJsonObject{}}
                }},
                {"timeout", QJsonObject{
                    {"type", "integer"},
                    {"description", "Optional. The maximum time to wait for execution, in milliseconds. Defaults to 30000"}
                }}
            }}
        },
        [&tidewaveBridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            QJsonObject result = tidewaveBridge.executeCommand("project_eval", params);

            if (result.contains("error")) {
                QString error = result["message"].toString();
// Logging disabled for non-chromium tools:                 chromiumLogger.logActivity("tidewave_project_eval", requestId, params, "error", timer.elapsed(), error);
                return QJsonObject{
                    {"type", "text"},
                    {"text", error},
                    {"isError", true}
                };
            }

// Logging disabled for non-chromium tools:             chromiumLogger.logActivity("tidewave_project_eval", requestId, params, "success", timer.elapsed(), QString(), result);
            return tidewaveBridge.formatResponse(result);
        }
    });

    server.registerTool({
        "tau5_hydra_eval",
        "Updates the Hydra visual sketch running in the background iframe. Accepts Hydra.js code that will be executed in the browser.",
        QJsonObject{
            {"type", "object"},
            {"required", QJsonArray{"code"}},
            {"properties", QJsonObject{
                {"code", QJsonObject{
                    {"type", "string"},
                    {"description", "Hydra sketch code to run in the background iframe"}
                }}
            }}
        },
        [&bridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            QString hydraCode = params["code"].toString();

            // Escape the Hydra code for JavaScript string literal
            QString escapedCode = hydraCode;
            escapedCode.replace("\\", "\\\\");
            escapedCode.replace("\"", "\\\"");
            escapedCode.replace("\n", "\\n");
            escapedCode.replace("\r", "\\r");
            escapedCode.replace("\t", "\\t");

            // JavaScript to send the Hydra code to the iframe via postMessage
            QString jsExpression = QString(R"JS(
(() => {
    const iframe = document.getElementById('hydra-background');
    if (iframe && iframe.contentWindow) {
        iframe.contentWindow.postMessage({
            type: 'update_sketch',
            code: "%1"
        }, '*');
        return 'Hydra sketch updated successfully';
    } else {
        return 'Error: Hydra iframe not found';
    }
})()
            )JS").arg(escapedCode);

            QJsonObject result = bridge.executeCommand([jsExpression](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->evaluateJavaScript(jsExpression, cb);
            });

            qint64 duration = timer.elapsed();

            if (result.contains("exceptionDetails")) {
                QJsonObject exception = result["exceptionDetails"].toObject();
                QString errorText = exception["text"].toString();
                chromiumLogger.logActivity("tau5_hydra_eval", requestId, params, "exception", duration, errorText);
                return QJsonObject{
                    {"type", "text"},
                    {"text", QString("JavaScript exception: %1").arg(errorText)},
                    {"isError", true}
                };
            }

            // Extract the result
            QString resultText;
            if (result.contains("result")) {
                QJsonObject resultObj = result["result"].toObject();
                if (resultObj.contains("value")) {
                    resultText = resultObj["value"].toString();
                }
            }

            if (resultText.isEmpty()) {
                resultText = "Hydra sketch update attempted";
            }

            chromiumLogger.logActivity("tau5_hydra_eval", requestId, params, "success", duration, QString(), resultText);

            return QJsonObject{
                {"type", "text"},
                {"text", resultText}
            };
        }
    });

    server.registerTool({
        "tidewave_search_package_docs",
        "Searches Hex documentation for the project's dependencies or a list of packages. If you're trying to get documentation for a specific module or function, first try the project_eval tool with the h helper.",
        QJsonObject{
            {"type", "object"},
            {"required", QJsonArray{"q"}},
            {"properties", QJsonObject{
                {"q", QJsonObject{
                    {"type", "string"},
                    {"description", "The search query"}
                }},
                {"packages", QJsonObject{
                    {"type", "array"},
                    {"description", "Optional list of packages to search. Defaults to project dependencies."},
                    {"items", QJsonObject{{"type", "string"}}}
                }}
            }}
        },
        [&tidewaveBridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            QJsonObject result = tidewaveBridge.executeCommand("search_package_docs", params);

            if (result.contains("error")) {
                QString error = result["message"].toString();
// Logging disabled for non-chromium tools:                 chromiumLogger.logActivity("tidewave_search_package_docs", requestId, params, "error", timer.elapsed(), error);
                return QJsonObject{
                    {"type", "text"},
                    {"text", error}
                };
            }

// Logging disabled for non-chromium tools:             chromiumLogger.logActivity("tidewave_search_package_docs", requestId, params, "success", timer.elapsed(), QString(), result);
            return tidewaveBridge.formatResponse(result);
        }
    });

    server.registerTool({
        "tidewave_execute_sql_query",
        "Executes the given SQL query against the given default or specified Ecto repository. Returns the result as an Elixir data structure.",
        QJsonObject{
            {"type", "object"},
            {"required", QJsonArray{"query"}},
            {"properties", QJsonObject{
                {"query", QJsonObject{
                    {"type", "string"},
                    {"description", "The SQL query to execute"}
                }},
                {"repo", QJsonObject{
                    {"type", "string"},
                    {"description", "The Ecto repository module (optional, defaults to first configured repo)"}
                }},
                {"bindings", QJsonObject{
                    {"type", "array"},
                    {"description", "Optional query bindings"},
                    {"items", QJsonObject{}}
                }}
            }}
        },
        [&tidewaveBridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            QJsonObject result = tidewaveBridge.executeCommand("execute_sql_query", params);

            if (result.contains("error")) {
                QString error = result["message"].toString();
// Logging disabled for non-chromium tools:                 chromiumLogger.logActivity("tidewave_execute_sql_query", requestId, params, "error", timer.elapsed(), error);
                return QJsonObject{
                    {"type", "text"},
                    {"text", error}
                };
            }

// Logging disabled for non-chromium tools:             chromiumLogger.logActivity("tidewave_execute_sql_query", requestId, params, "success", timer.elapsed(), QString(), result);
            return tidewaveBridge.formatResponse(result);
        }
    });

    server.registerTool({
        "tidewave_get_ecto_schemas",
        "Returns information about Ecto schemas in the project",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"schema", QJsonObject{
                    {"type", "string"},
                    {"description", "Optional specific schema module to get information about"}
                }}
            }}
        },
        [&tidewaveBridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            QJsonObject result = tidewaveBridge.executeCommand("get_ecto_schemas", params);

            if (result.contains("error")) {
                QString error = result["message"].toString();
// Logging disabled for non-chromium tools:                 chromiumLogger.logActivity("tidewave_get_ecto_schemas", requestId, params, "error", timer.elapsed(), error);
                return QJsonObject{
                    {"type", "text"},
                    {"text", error}
                };
            }

// Logging disabled for non-chromium tools:             chromiumLogger.logActivity("tidewave_get_ecto_schemas", requestId, params, "success", timer.elapsed(), QString(), result);
            return tidewaveBridge.formatResponse(result);
        }
    });

    server.registerTool({
        "tidewave_call_tool",
        "Call any Tidewave MCP tool directly. This is a generic proxy for tools that may be added to Tidewave in the future.",
        QJsonObject{
            {"type", "object"},
            {"required", QJsonArray{"name", "arguments"}},
            {"properties", QJsonObject{
                {"name", QJsonObject{
                    {"type", "string"},
                    {"description", "The name of the Tidewave tool to call"}
                }},
                {"arguments", QJsonObject{
                    {"type", "object"},
                    {"description", "The arguments to pass to the tool"},
                    {"additionalProperties", true}
                }}
            }}
        },
        [&tidewaveBridge, &chromiumLogger](const QJsonObject& params) -> QJsonObject {
            QString requestId = QUuid::createUuid().toString();
            QElapsedTimer timer;
            timer.start();

            QString toolName = params["name"].toString();
            QJsonObject arguments = params["arguments"].toObject();

            QJsonObject result = tidewaveBridge.executeCommand(toolName, arguments);

            if (result.contains("error")) {
                QString error = result["message"].toString();
// Logging disabled for non-chromium tools:                 chromiumLogger.logActivity("tidewave_call_tool", requestId, params, "error", timer.elapsed(), error);
                return QJsonObject{
                    {"type", "text"},
                    {"text", error},
                    {"isError", true}
                };
            }

// Logging disabled for non-chromium tools:             chromiumLogger.logActivity("tidewave_call_tool", requestId, params, "success", timer.elapsed(), QString(), result);
            return tidewaveBridge.formatResponse(result);
        }
    });

    QObject::connect(&server, &MCPServerStdio::stdinClosed, &app, [&app]() {
        debugLog("Stdin closed, shutting down MCP server...");
        QTimer::singleShot(100, &app, &QCoreApplication::quit);
    });
    
    server.start();
    
    debugLog("MCP server ready. Starting pre-emptive CDP connection...");
    
    // Pre-emptively start the connection process to reduce first-request latency
    QTimer::singleShot(500, [&bridge]() {
        debugLog("Starting pre-emptive CDP connection attempt");
        bridge.ensureConnected();
    });
    
    QObject::connect(cdpClient.get(), &CDPClient::connected, []() {
        debugLog("Successfully connected to Chrome DevTools");
    });
    
    
    QObject::connect(cdpClient.get(), &CDPClient::consoleMessage,
                     [](const QString& level, const QString& text) {
        debugLog(QString("[Console %1] %2").arg(level).arg(text));
    });
    
    return app.exec();
}