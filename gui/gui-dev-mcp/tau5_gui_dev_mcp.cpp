#include <QCoreApplication>
#include <QTimer>
#include <QEventLoop>
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
    
    QJsonObject executeCommand(std::function<void(CDPClient*, CDPClient::ResponseCallback)> command)
    {
        try {
            if (!m_client->isConnected()) {
                debugLog("CDP not connected, attempting to connect...");
                m_client->connect();
                
                if (!waitForConnection(2000)) {
                    debugLog("CDP connection timeout - Tau5 may not be running");
                    return createErrorResult("Chrome DevTools not responding. Make sure Tau5 is running in dev mode with --remote-debugging-port=9223");
                }
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
                return createErrorResult("CDP command timed out");
            }
            
            if (!error.isEmpty()) {
                return createErrorResult(error);
            }
            
            return result;
        } catch (const std::exception& e) {
            return createErrorResult(QString("Exception: %1").arg(e.what()));
        } catch (...) {
            return createErrorResult("Unknown exception occurred");
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
            
            QJsonObject result = bridge.executeCommand([expression](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->evaluateJavaScript(expression, cb);
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
            
            QString resultText;
            if (value.isString()) {
                resultText = value.toString();
            } else if (value.isDouble()) {
                resultText = QString::number(value.toDouble());
            } else if (value.isBool()) {
                resultText = value.toBool() ? "true" : "false";
            } else if (value.isObject() || value.isArray()) {
                QJsonDocument doc(value.toObject());
                resultText = doc.toJson(QJsonDocument::Indented);
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
                }}
            }},
            {"required", QJsonArray{"selector"}}
        },
        [&bridge, &cdpClient](const QJsonObject& params) -> QJsonObject {
            QString selector = params["selector"].toString();
            
            QString jsExpression = QString(R"(
                (function() {
                    const element = document.querySelector('%1');
                    if (!element) return { error: 'Element not found' };
                    const styles = window.getComputedStyle(element);
                    const result = {};
                    for (let i = 0; i < styles.length; i++) {
                        const prop = styles[i];
                        result[prop] = styles.getPropertyValue(prop);
                    }
                    return result;
                })()
            )").arg(selector.replace("'", "\\'"));
            
            QJsonObject result = bridge.executeCommand([jsExpression](CDPClient* client, CDPClient::ResponseCallback cb) {
                client->evaluateJavaScript(jsExpression, cb);
            });
            
            if (result.contains("type") && result["type"].toString() == "text") {
                return result;
            }
            
            QJsonObject resultObj = result["result"].toObject();
            QJsonValue value = resultObj["value"];
            
            if (value.isObject() && value.toObject().contains("error")) {
                return QJsonObject{
                    {"type", "text"},
                    {"text", value.toObject()["error"].toString()}
                };
            }
            
            QJsonDocument doc(value.toObject());
            QJsonObject styleResult;
            styleResult["type"] = "text";
            styleResult["text"] = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
            return styleResult;
        }
    });
    
    QObject::connect(&server, &MCPServerStdio::stdinClosed, &app, [&app]() {
        debugLog("Stdin closed, shutting down MCP server...");
        QTimer::singleShot(100, &app, &QCoreApplication::quit);
    });
    
    server.start();
    
    debugLog("MCP server ready. CDP connection will be attempted when first tool is called.");
    
    QObject::connect(cdpClient.get(), &CDPClient::connected, []() {
        debugLog("Successfully connected to Chrome DevTools");
    });
    
    
    QObject::connect(cdpClient.get(), &CDPClient::consoleMessage,
                     [](const QString& level, const QString& text) {
        debugLog(QString("[Console %1] %2").arg(level).arg(text));
    });
    
    return app.exec();
}