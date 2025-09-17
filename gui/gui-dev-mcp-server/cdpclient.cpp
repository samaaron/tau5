#include "cdpclient.h"
#include <iostream>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>
#include <QTimer>

CDPClient::CDPClient(quint16 devToolsPort, QObject* parent)
    : QObject(parent)
    , m_devToolsPort(devToolsPort)
    , m_webSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_pingTimer(new QTimer(this))
    , m_nextCommandId(1)
    , m_isConnecting(false)
    , m_isConnected(false)
    , m_connectionState(ConnectionState::NotConnected)
{
    QObject::connect(m_webSocket, &QWebSocket::connected, this, &CDPClient::onConnected);
    QObject::connect(m_webSocket, &QWebSocket::disconnected, this, &CDPClient::onDisconnected);
    QObject::connect(m_webSocket, &QWebSocket::textMessageReceived, this, &CDPClient::onTextMessageReceived);
    
    m_pingTimer->setInterval(PING_INTERVAL_MS);
    QObject::connect(m_pingTimer, &QTimer::timeout, this, &CDPClient::onPingTimeout);
}

CDPClient::~CDPClient()
{
    disconnect();
}

bool CDPClient::connect()
{
    if (m_isConnected) {
        return true;
    }
    
    if (m_isConnecting) {
        return false;
    }
    
    disconnect();
    
    m_isConnecting = true;
    m_connectionState = ConnectionState::Connecting;
    std::cerr << "# CDP: Connecting to Chrome DevTools Protocol on port " << m_devToolsPort << std::endl;
    
    fetchTargetList();
    
    return false;
}

void CDPClient::disconnect()
{
    if (m_webSocket->state() != QAbstractSocket::UnconnectedState) {
        m_webSocket->close();
    }
    m_pingTimer->stop();
    m_pendingCommands.clear();
    m_isConnected = false;
    m_isConnecting = false;
    m_connectionState = ConnectionState::NotConnected;
    m_webSocketDebuggerUrl.clear();
    m_targetId.clear();
}

bool CDPClient::isConnected() const
{
    return m_isConnected;
}

CDPClient::ConnectionState CDPClient::getConnectionState() const
{
    return m_connectionState;
}

void CDPClient::fetchTargetList()
{
    QUrl url(QString("http://localhost:%1/json/list").arg(m_devToolsPort));
    QNetworkRequest request(url);
    
    QNetworkReply* reply = m_networkManager->get(request);
    QObject::connect(reply, &QNetworkReply::finished, [this, reply]() {
        reply->deleteLater();
        
        if (reply->error() != QNetworkReply::NoError) {
            QString errorMsg = QString("Cannot connect to Chrome DevTools on port %1: %2").arg(m_devToolsPort).arg(reply->errorString());
            std::cerr << "# CDP Error: " << errorMsg.toStdString() << std::endl;
            m_isConnecting = false;
            m_isConnected = false;
            m_connectionState = ConnectionState::Failed;
            if (m_webSocket->state() != QAbstractSocket::UnconnectedState) {
                m_webSocket->abort();
            }
            emit connectionFailed(errorMsg);
            return;
        }
        
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) {
            QString errorMsg = "Invalid DevTools target list format - Tau5 may not be running";
            std::cerr << "# CDP Error: " << errorMsg.toStdString() << std::endl;
            m_isConnecting = false;
            m_isConnected = false;
            m_connectionState = ConnectionState::Failed;
            if (m_webSocket->state() != QAbstractSocket::UnconnectedState) {
                m_webSocket->abort();
            }
            emit connectionFailed(errorMsg);
            return;
        }
        
        QJsonArray targets = doc.array();
        QString targetId = findMainPageTarget(targets);
        
        if (targetId.isEmpty()) {
            QString errorMsg = "No suitable DevTools target found - check if Tau5 is running in dev mode";
            std::cerr << "# CDP Error: " << errorMsg.toStdString() << std::endl;
            m_isConnecting = false;
            m_isConnected = false;
            if (m_webSocket->state() != QAbstractSocket::UnconnectedState) {
                m_webSocket->abort();
            }
            emit connectionFailed(errorMsg);
            return;
        }
        
        for (const QJsonValue& value : targets) {
            QJsonObject target = value.toObject();
            if (target["id"].toString() == targetId) {
                m_webSocketDebuggerUrl = target["webSocketDebuggerUrl"].toString();
                break;
            }
        }
        
        if (m_webSocketDebuggerUrl.isEmpty()) {
            QString errorMsg = "No WebSocket debugger URL found - ensure Tau5 is running with DevTools enabled";
            std::cerr << "# CDP Error: " << errorMsg.toStdString() << std::endl;
            m_isConnecting = false;
            m_isConnected = false;
            if (m_webSocket->state() != QAbstractSocket::UnconnectedState) {
                m_webSocket->abort();
            }
            emit connectionFailed(errorMsg);
            return;
        }
        
        connectToTarget(targetId);
    });
}

QString CDPClient::findMainPageTarget(const QJsonArray& targets)
{
    for (const QJsonValue& value : targets) {
        QJsonObject target = value.toObject();
        QString type = target["type"].toString();
        QString url = target["url"].toString();
        
        if (type == "page" && !url.contains("devtools://")) {
            std::cerr << "# CDP: Found main page target: " << target["title"].toString().toStdString() << std::endl;
            return target["id"].toString();
        }
    }
    
    for (const QJsonValue& value : targets) {
        QJsonObject target = value.toObject();
        if (target["type"].toString() == "page") {
            return target["id"].toString();
        }
    }
    
    return QString();
}

void CDPClient::connectToTarget(const QString& targetId)
{
    m_targetId = targetId;
    
    std::cerr << "# CDP: Connecting to DevTools WebSocket: " << m_webSocketDebuggerUrl.toStdString() << std::endl;
    
    m_webSocket->open(QUrl(m_webSocketDebuggerUrl));
}

void CDPClient::onConnected()
{
    std::cerr << "# CDP: Connected to Chrome DevTools Protocol" << std::endl;
    m_isConnected = true;
    m_isConnecting = false;
    m_connectionState = ConnectionState::Connected;
    m_pingTimer->start();
    
    enableDomains();
    
    emit connected();
    emit logMessage("CDP Client connected");
}

void CDPClient::onDisconnected()
{
    std::cerr << "# CDP: Disconnected from Chrome DevTools Protocol" << std::endl;
    m_isConnected = false;
    m_isConnecting = false;
    m_connectionState = ConnectionState::NotConnected;
    m_pingTimer->stop();
    
    for (auto it = m_pendingCommands.begin(); it != m_pendingCommands.end(); ++it) {
        it.value()(QJsonObject(), "Connection lost");
    }
    m_pendingCommands.clear();
    
    emit disconnected();
    emit logMessage("CDP Client disconnected");
}

void CDPClient::onTextMessageReceived(const QString& message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        std::cerr << "# CDP Warning: Received invalid CDP message" << std::endl;
        return;
    }
    
    processResponse(doc.object());
}

void CDPClient::onPingTimeout()
{
    sendCommand("Runtime.evaluate", QJsonObject{{"expression", "1"}}, [](const QJsonObject&, const QString&) {});
}

void CDPClient::processResponse(const QJsonObject& response)
{
    if (response.contains("method")) {
        QString method = response["method"].toString();
        QJsonObject params = response["params"].toObject();
        
        if (method == "Runtime.consoleAPICalled") {
            QString level = params["type"].toString();
            QJsonArray args = params["args"].toArray();

            // Build text representation for backward compatibility
            QString text;
            for (const QJsonValue& arg : args) {
                QJsonObject argObj = arg.toObject();
                QString type = argObj["type"].toString();
                if (type == "string") {
                    text += argObj["value"].toString() + " ";
                } else if (type == "number" || type == "boolean") {
                    text += argObj["value"].toVariant().toString() + " ";
                } else if (type == "object") {
                    // Handle objects and errors
                    QString className = argObj["className"].toString();
                    QString description = argObj["description"].toString();
                    if (!description.isEmpty()) {
                        text += description + " ";
                    } else if (!className.isEmpty()) {
                        text += "[" + className + "] ";
                    } else {
                        text += "[object] ";
                    }
                } else if (type == "undefined") {
                    text += "undefined ";
                }
            }

            // Handle console.time/timeEnd
            if (level == "timeEnd" && args.size() > 0) {
                QString label = args[0].toObject()["value"].toString();
                if (m_performanceTimers.contains(label)) {
                    qint64 startTime = m_performanceTimers.take(label);
                    qint64 duration = QDateTime::currentMSecsSinceEpoch() - startTime;
                    text = QString("%1: %2ms").arg(label).arg(duration);
                }
            } else if (level == "time" && args.size() > 0) {
                QString label = args[0].toObject()["value"].toString();
                m_performanceTimers[label] = QDateTime::currentMSecsSinceEpoch();
            }

            // Extract stack trace and source location
            QString stackTrace;
            QString url;
            int lineNumber = 0;
            int columnNumber = 0;
            QString functionName;

            if (params.contains("stackTrace")) {
                QJsonArray callFrames = params["stackTrace"].toObject()["callFrames"].toArray();
                if (!callFrames.isEmpty()) {
                    // Get source location from first frame
                    QJsonObject firstFrame = callFrames[0].toObject();
                    url = firstFrame["url"].toString();
                    lineNumber = firstFrame["lineNumber"].toInt();
                    columnNumber = firstFrame["columnNumber"].toInt();
                    functionName = firstFrame["functionName"].toString();
                    if (functionName.isEmpty()) functionName = "<anonymous>";
                }

                // Build stack trace string
                for (const QJsonValue& frame : callFrames) {
                    QJsonObject frameObj = frame.toObject();
                    QString fname = frameObj["functionName"].toString();
                    if (fname.isEmpty()) fname = "<anonymous>";
                    stackTrace += QString("    at %1 (%2:%3:%4)\n")
                        .arg(fname)
                        .arg(frameObj["url"].toString())
                        .arg(frameObj["lineNumber"].toInt())
                        .arg(frameObj["columnNumber"].toInt());
                }
            }

            // Store the message with all metadata
            ConsoleMessage msg;
            msg.timestamp = QDateTime::currentDateTime();
            msg.level = level;
            msg.text = text.trimmed();
            msg.stackTrace = stackTrace;
            msg.url = url;
            msg.lineNumber = lineNumber;
            msg.columnNumber = columnNumber;
            msg.functionName = functionName;
            msg.args = args;  // Preserve structured data
            msg.groupId = "";  // Will be set for group messages
            msg.isGroupStart = (level == "group" || level == "groupCollapsed");
            msg.isGroupEnd = (level == "groupEnd");

            m_consoleMessages.append(msg);

            // Keep only the last MAX_CONSOLE_MESSAGES
            while (m_consoleMessages.size() > MAX_CONSOLE_MESSAGES) {
                m_consoleMessages.removeFirst();
            }

            emit consoleMessage(level, text.trimmed());
        } else if (method == "DOM.documentUpdated") {
            emit domContentUpdated();
        }
        
        return;
    }
    
    if (response.contains("id")) {
        int id = response["id"].toInt();
        
        if (m_pendingCommands.contains(id)) {
            ResponseCallback callback = m_pendingCommands.take(id);
            
            if (response.contains("error")) {
                QJsonObject error = response["error"].toObject();
                QString errorMessage = error["message"].toString();
                callback(QJsonObject(), errorMessage);
            } else {
                QJsonObject result = response.value("result").toObject();
                callback(result, QString());
            }
        }
    }
}

void CDPClient::enableDomains()
{
    sendCommand("DOM.enable", QJsonObject(), [](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            std::cerr << "# CDP Warning: Failed to enable DOM domain: " << error.toStdString() << std::endl;
        }
    });
    
    sendCommand("Runtime.enable", QJsonObject(), [](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            std::cerr << "# CDP Warning: Failed to enable Runtime domain: " << error.toStdString() << std::endl;
        }
    });
    
    sendCommand("Page.enable", QJsonObject(), [](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            std::cerr << "# CDP Warning: Failed to enable Page domain: " << error.toStdString() << std::endl;
        }
    });
}

void CDPClient::sendCommand(const QString& method, const QJsonObject& params, ResponseCallback callback)
{
    if (!m_isConnected && !m_isConnecting) {
        callback(QJsonObject(), "Not connected to Chrome DevTools. Ensure Tau5 is running with --remote-debugging-port=9223");
        return;
    }
    
    if (!m_isConnected && m_isConnecting) {
        callback(QJsonObject(), "Chrome DevTools connection in progress. Please try again in a moment.");
        return;
    }
    
    int commandId = m_nextCommandId++;
    m_pendingCommands[commandId] = callback;
    
    QJsonObject command{
        {"id", commandId},
        {"method", method},
        {"params", params}
    };
    
    sendRawCommand(command);
}

void CDPClient::sendRawCommand(const QJsonObject& command)
{
    QJsonDocument doc(command);
    QString message = doc.toJson(QJsonDocument::Compact);
    
    // Debug logging disabled for MCP server to avoid noise
    // std::cerr << "# CDP -> " << message.toStdString() << std::endl;
    m_webSocket->sendTextMessage(message);
}

void CDPClient::getDocument(ResponseCallback callback)
{
    sendCommand("DOM.getDocument", QJsonObject{{"depth", -1}, {"pierce", true}}, callback);
}

void CDPClient::querySelector(const QString& selector, ResponseCallback callback)
{
    getDocument([this, selector, callback](const QJsonObject& result, const QString& error) {
        if (!error.isEmpty()) {
            callback(QJsonObject(), error);
            return;
        }
        
        int rootNodeId = result["root"].toObject()["nodeId"].toInt();
        QJsonObject params{
            {"nodeId", rootNodeId},
            {"selector", selector}
        };
        
        sendCommand("DOM.querySelector", params, callback);
    });
}

void CDPClient::getOuterHTML(int nodeId, ResponseCallback callback)
{
    QJsonObject params{{"nodeId", nodeId}};
    sendCommand("DOM.getOuterHTML", params, callback);
}

void CDPClient::evaluateJavaScript(const QString& expression, ResponseCallback callback)
{
    QJsonObject params{
        {"expression", expression},
        {"returnByValue", true},
        {"awaitPromise", true}
    };
    
    sendCommand("Runtime.evaluate", params, callback);
}

void CDPClient::evaluateJavaScriptWithObjectReferences(const QString& expression, ResponseCallback callback)
{
    QJsonObject params{
        {"expression", expression},
        {"returnByValue", false},
        {"awaitPromise", true},
        {"generatePreview", true}
    };
    
    sendCommand("Runtime.evaluate", params, callback);
}

void CDPClient::getConsoleMessages(const QJsonObject& filters, ResponseCallback callback)
{
    QJsonArray messages;

    // Parse filter parameters
    QStringList levelFilter;
    if (filters.contains("level")) {
        QJsonValue levelValue = filters["level"];
        if (levelValue.isString()) {
            levelFilter << levelValue.toString();
        } else if (levelValue.isArray()) {
            for (const QJsonValue& val : levelValue.toArray()) {
                levelFilter << val.toString();
            }
        }
    }

    QString searchPattern = filters.value("search").toString();
    QString regexPattern = filters.value("regex").toString();
    QRegularExpression regex;
    if (!regexPattern.isEmpty()) {
        regex = QRegularExpression(regexPattern);
    }

    // Time-based filtering
    QDateTime sinceTime;
    if (filters.contains("since")) {
        sinceTime = QDateTime::fromString(filters["since"].toString(), Qt::ISODate);
    }

    if (filters.contains("last")) {
        QString lastStr = filters["last"].toString();
        int minutes = 0;
        if (lastStr.endsWith("m")) {
            minutes = lastStr.chopped(1).toInt();
        } else if (lastStr.endsWith("h")) {
            minutes = lastStr.chopped(1).toInt() * 60;
        } else if (lastStr.endsWith("s")) {
            minutes = lastStr.chopped(1).toInt() / 60;
        }
        if (minutes > 0) {
            sinceTime = QDateTime::currentDateTime().addSecs(-minutes * 60);
        }
    }

    // Since last call option
    bool sinceLastCall = filters.value("since_last_call").toBool();
    if (sinceLastCall && m_lastMessageRetrievalTime.isValid()) {
        sinceTime = m_lastMessageRetrievalTime;
    }

    // Output format
    QString format = filters.value("format").toString("json");

    // Limit
    int limit = filters.value("limit").toInt(-1);

    // Filter and build messages
    for (const ConsoleMessage& msg : m_consoleMessages) {
        // Level filter
        if (!levelFilter.isEmpty() && !levelFilter.contains(msg.level)) {
            continue;
        }

        // Time filter
        if (sinceTime.isValid() && msg.timestamp < sinceTime) {
            continue;
        }

        // Search filter
        if (!searchPattern.isEmpty() && !msg.text.contains(searchPattern, Qt::CaseInsensitive)) {
            continue;
        }

        // Regex filter
        if (!regexPattern.isEmpty() && !regex.match(msg.text).hasMatch()) {
            continue;
        }

        QJsonObject msgObj;
        msgObj["timestamp"] = msg.timestamp.toString(Qt::ISODateWithMs);
        msgObj["level"] = msg.level;
        msgObj["text"] = msg.text;

        // Add source location if available
        if (!msg.url.isEmpty()) {
            msgObj["url"] = msg.url;
            msgObj["lineNumber"] = msg.lineNumber;
            msgObj["columnNumber"] = msg.columnNumber;
            if (!msg.functionName.isEmpty()) {
                msgObj["functionName"] = msg.functionName;
            }
        }

        // Add structured arguments
        if (!msg.args.isEmpty()) {
            msgObj["args"] = msg.args;
        }

        // Add stack trace
        if (!msg.stackTrace.isEmpty()) {
            msgObj["stackTrace"] = msg.stackTrace;
        }

        // Group support
        if (msg.isGroupStart) {
            msgObj["groupStart"] = true;
        }
        if (msg.isGroupEnd) {
            msgObj["groupEnd"] = true;
        }
        if (!msg.groupId.isEmpty()) {
            msgObj["groupId"] = msg.groupId;
        }

        messages.append(msgObj);

        // Apply limit if specified
        if (limit > 0 && messages.size() >= limit) {
            break;
        }
    }

    // Update last retrieval time
    if (sinceLastCall) {
        m_lastMessageRetrievalTime = QDateTime::currentDateTime();
    }

    QJsonObject result;
    result["messages"] = messages;
    result["count"] = messages.size();
    result["format"] = format;

    callback(result, QString());
}

void CDPClient::clearConsoleMessages()
{
    m_consoleMessages.clear();
    m_performanceTimers.clear();
}

void CDPClient::markMessageRetrievalTime()
{
    m_lastMessageRetrievalTime = QDateTime::currentDateTime();
}

void CDPClient::navigateTo(const QString& url, ResponseCallback callback)
{
    QJsonObject params{{"url", url}};
    sendCommand("Page.navigate", params, callback);
}

void CDPClient::setAttributeValue(int nodeId, const QString& name, const QString& value, ResponseCallback callback)
{
    QJsonObject params{
        {"nodeId", nodeId},
        {"name", name},
        {"value", value}
    };
    
    sendCommand("DOM.setAttributeValue", params, callback);
}

void CDPClient::removeAttribute(int nodeId, const QString& name, ResponseCallback callback)
{
    QJsonObject params{
        {"nodeId", nodeId},
        {"name", name}
    };
    
    sendCommand("DOM.removeAttribute", params, callback);
}

void CDPClient::setOuterHTML(int nodeId, const QString& html, ResponseCallback callback)
{
    QJsonObject params{
        {"nodeId", nodeId},
        {"outerHTML", html}
    };
    
    sendCommand("DOM.setOuterHTML", params, callback);
}

void CDPClient::getProperties(const QString& objectId, ResponseCallback callback)
{
    QJsonObject params{
        {"objectId", objectId},
        {"ownProperties", true},
        {"accessorPropertiesOnly", false},
        {"generatePreview", true}
    };
    
    sendCommand("Runtime.getProperties", params, callback);
}

void CDPClient::callFunctionOn(const QString& objectId, const QString& functionDeclaration, ResponseCallback callback)
{
    QJsonObject params{
        {"objectId", objectId},
        {"functionDeclaration", functionDeclaration},
        {"returnByValue", false},
        {"awaitPromise", true},
        {"generatePreview", true}
    };
    
    sendCommand("Runtime.callFunctionOn", params, callback);
}

void CDPClient::releaseObject(const QString& objectId, ResponseCallback callback)
{
    QJsonObject params{
        {"objectId", objectId}
    };
    
    sendCommand("Runtime.releaseObject", params, callback);
}

void CDPClient::discoverTargets()
{
    fetchTargetList();
}