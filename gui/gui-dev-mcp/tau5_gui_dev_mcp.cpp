#include <QCoreApplication>
#include <QTimer>
#include <QEventLoop>
#include <QThread>
#include <iostream>
#include <memory>
#include "mcpserver_stdio.h"
#include "cdpclient.h"

static void debugLog(const QString& message) {
    std::cerr << "# " << message.toStdString() << std::endl;
}

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
                    return createErrorResult("Chrome DevTools not responding after multiple attempts. Make sure Tau5 is running in dev mode with --remote-debugging-port=9223");
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

#include "tau5_gui_dev_mcp.moc"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("tau5-gui-dev-mcp");
    app.setOrganizationName("Tau5");
    
    quint16 devToolsPort = 9223;
    bool debugMode = false;
    
    for (int i = 1; i < argc; i++) {
        QString arg = QString::fromUtf8(argv[i]);
        if (arg == "--devtools-port" && i + 1 < argc) {
            devToolsPort = QString::fromUtf8(argv[++i]).toUInt();
        } else if (arg == "--debug") {
            debugMode = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Tau5 GUI Dev MCP Server\n\n";
            std::cout << "This server provides MCP (Model Context Protocol) access to Chrome DevTools.\n";
            std::cout << "It connects to a running Tau5 instance with DevTools enabled.\n\n";
            std::cout << "Usage: tau5-gui-dev-mcp [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --devtools-port <port>  Chrome DevTools port (default: 9223)\n";
            std::cout << "  --debug                 Enable debug logging to tau5-mcp-debug.log\n";
            std::cout << "  --help, -h              Show this help message\n\n";
            std::cout << "Configure in Claude Code with:\n";
            std::cout << "  \"mcpServers\": {\n";
            std::cout << "    \"tau5-gui-dev\": {\n";
            std::cout << "      \"command\": \"path/to/tau5-gui-dev-mcp\",\n";
            std::cout << "      \"args\": [\"--devtools-port\", \"9223\"]\n";
            std::cout << "    }\n";
            std::cout << "  }\n";
            return 0;
        }
    }
    
    debugLog("Tau5 GUI Dev MCP Server v1.0.0");
    debugLog(QString("Connecting to Chrome DevTools on port %1").arg(devToolsPort));
    
    MCPServerStdio server;
    server.setServerInfo("Tau5 GUI Dev MCP", "1.0.0");
    server.setCapabilities(QJsonObject{
        {"tools", QJsonObject{}}
    });
    server.setDebugMode(debugMode);
    
    auto cdpClient = std::make_unique<CDPClient>(devToolsPort);
    
    CDPBridge bridge(cdpClient.get());
    
    QObject::connect(cdpClient.get(), &CDPClient::disconnected, [&cdpClient]() {
        debugLog("CDP Client disconnected - Tau5 may not be running");
    });
    
    QObject::connect(cdpClient.get(), &CDPClient::connectionFailed, [](const QString& error) {
        debugLog(QString("CDP connection error: %1").arg(error));
    });
    
    server.registerTool({
        "chromium_devtools_getDocument",
        "Get the full DOM document structure",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{}}
        },
        [&bridge](const QJsonObject& params) -> QJsonObject {
            Q_UNUSED(params);
            
            QJsonObject result = bridge.executeCommand([](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getDocument(cb);
            });
            
            if (result.contains("type") && result["type"].toString() == "text") {
                return result;
            }
            
            QJsonDocument doc(result);
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
        [&bridge](const QJsonObject& params) -> QJsonObject {
            QString selector = params["selector"].toString();
            
            QJsonObject result = bridge.executeCommand([selector](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->querySelector(selector, cb);
            });
            
            if (result.contains("type") && result["type"].toString() == "text") {
                return result;
            }
            
            int nodeId = result["nodeId"].toInt();
            if (nodeId == 0) {
                return QJsonObject{
                    {"type", "text"},
                    {"text", QString("No element found matching selector: %1").arg(selector)}
                };
            }
            
            return QJsonObject{
                {"type", "text"},
                {"text", QString("Found element with nodeId: %1").arg(nodeId)}
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
        [&bridge](const QJsonObject& params) -> QJsonObject {
            int nodeId = params["nodeId"].toInt();
            
            QJsonObject result = bridge.executeCommand([nodeId](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getOuterHTML(nodeId, cb);
            });
            
            if (result.contains("type") && result["type"].toString() == "text") {
                return result;
            }
            
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
        [&bridge](const QJsonObject& params) -> QJsonObject {
            QString expression = params["expression"].toString();
            
            // First try with object references to avoid serialization issues
            QJsonObject result = bridge.executeCommand([expression](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->evaluateJavaScriptWithObjectReferences(expression, cb);
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
            
            return QJsonObject{
                {"type", "text"},
                {"text", resultText}
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
        "Navigate to a URL",
        QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"url", QJsonObject{
                    {"type", "string"}, 
                    {"description", "URL to navigate to"}
                }}
            }},
            {"required", QJsonArray{"url"}}
        },
        [&bridge](const QJsonObject& params) -> QJsonObject {
            QString url = params["url"].toString();
            
            QJsonObject result = bridge.executeCommand([url](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->navigateTo(url, cb);
            });
            
            if (result.contains("type") && result["type"].toString() == "text") {
                return result;
            }
            
            return QJsonObject{
                {"type", "text"},
                {"text", QString("Navigated to: %1").arg(url)}
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
        [&bridge](const QJsonObject& params) -> QJsonObject {
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
            
            if (result.contains("type") && result["type"].toString() == "text") {
                return result;
            }
            
            QJsonObject resultObj = result["result"].toObject();
            QJsonValue value = resultObj["value"];
            
            if (value.isObject() && value.toObject().contains("error")) {
                if (rawJson) {
                    return QJsonObject{{"error", value.toObject()["error"].toString()}};
                }
                return QJsonObject{
                    {"type", "text"},
                    {"text", value.toObject()["error"].toString()}
                };
            }
            
            return bridge.formatResponse(value, rawJson);
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
        [&bridge](const QJsonObject& params) -> QJsonObject {
            QString objectId = params["objectId"].toString();
            
            QJsonObject result = bridge.executeCommand([objectId](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->getProperties(objectId, cb);
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