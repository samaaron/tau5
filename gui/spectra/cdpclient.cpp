#include "cdpclient.h"
#include <iostream>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>
#include <QTimer>
#include <QCoreApplication>

CDPClient::CDPClient(quint16 devToolsPort, QObject* parent)
    : QObject(parent)
    , m_devToolsPort(devToolsPort)
    , m_webSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_pingTimer(new QTimer(this))
    , m_nextCommandId(1)
    , m_targetTitle("Tau5")
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
            m_connectionState = ConnectionState::NotConnected;
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
            m_connectionState = ConnectionState::NotConnected;
            if (m_webSocket->state() != QAbstractSocket::UnconnectedState) {
                m_webSocket->abort();
            }
            emit connectionFailed(errorMsg);
            return;
        }
        
        QJsonArray targets = doc.array();
        m_lastTargetList = targets;  // Cache the targets list
        QString targetId = findMainPageTarget(targets);

        if (targetId.isEmpty()) {
            QString errorMsg = "No suitable DevTools target found - check if Tau5 is running in dev mode";
            std::cerr << "# CDP Error: " << errorMsg.toStdString() << std::endl;
            m_isConnecting = false;
            m_isConnected = false;
            m_connectionState = ConnectionState::NotConnected;
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
                m_currentTargetTitle = target["title"].toString();  // Store current title
                break;
            }
        }
        
        if (m_webSocketDebuggerUrl.isEmpty()) {
            QString errorMsg = "No WebSocket debugger URL found - ensure Tau5 is running with DevTools enabled";
            std::cerr << "# CDP Error: " << errorMsg.toStdString() << std::endl;
            m_isConnecting = false;
            m_isConnected = false;
            m_connectionState = ConnectionState::NotConnected;
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
    // Look for page with the target title (defaults to "Tau5")
    for (const QJsonValue& value : targets) {
        QJsonObject target = value.toObject();
        QString type = target["type"].toString();
        QString title = target["title"].toString();
        QString url = target["url"].toString();

        if (type == "page" && title == m_targetTitle) {
            std::cerr << "# CDP: Found target with title '" << m_targetTitle.toStdString()
                      << "' at " << url.toStdString() << std::endl;
            return target["id"].toString();
        }
    }

    std::cerr << "# CDP: Error - Target with title '" << m_targetTitle.toStdString()
              << "' not found among available targets" << std::endl;
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
        } else if (method == "Log.entryAdded") {
            // Handle browser-generated log messages (network errors, violations, etc.)
            QJsonObject entry = params["entry"].toObject();

            QString level = entry["level"].toString(); // "verbose", "info", "warning", "error"
            QString source = entry["source"].toString(); // "xml", "javascript", "network", "console-api", etc.
            QString text = entry["text"].toString();
            QString url = entry["url"].toString();
            int lineNumber = entry["lineNumber"].toInt();

            // Map Log levels to console levels for consistency
            if (level == "verbose") level = "debug";

            // Store the message with metadata
            ConsoleMessage msg;
            msg.timestamp = QDateTime::currentDateTime();
            msg.level = level;
            msg.text = text;
            msg.stackTrace = entry["stackTrace"].toObject().isEmpty() ? "" : ""; // Could expand if needed
            msg.url = url;
            msg.lineNumber = lineNumber;
            msg.columnNumber = 0;
            msg.functionName = "";
            msg.args = QJsonArray(); // Log entries don't have structured args
            msg.groupId = "";
            msg.isGroupStart = false;
            msg.isGroupEnd = false;

            m_consoleMessages.append(msg);

            // Keep only the last MAX_CONSOLE_MESSAGES
            while (m_consoleMessages.size() > MAX_CONSOLE_MESSAGES) {
                m_consoleMessages.removeFirst();
            }

            emit consoleMessage(level, text);
        } else if (method == "DOM.documentUpdated") {
            emit domContentUpdated();
        } else if (method.startsWith("Network.")) {
            handleNetworkEvent(method, params);
        } else if (method.startsWith("Runtime.exception")) {
            handleRuntimeException(method, params);
        } else if (method.startsWith("Network.webSocket")) {
            handleWebSocketEvent(method, params);
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

    sendCommand("Log.enable", QJsonObject(), [](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            std::cerr << "# CDP Warning: Failed to enable Log domain: " << error.toStdString() << std::endl;
        }
    });

    sendCommand("Page.enable", QJsonObject(), [](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            std::cerr << "# CDP Warning: Failed to enable Page domain: " << error.toStdString() << std::endl;
        }
    });

    sendCommand("Network.enable", QJsonObject(), [](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            std::cerr << "# CDP Warning: Failed to enable Network domain: " << error.toStdString() << std::endl;
        }
    });

    sendCommand("Security.enable", QJsonObject(), [](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            std::cerr << "# CDP Warning: Failed to enable Security domain: " << error.toStdString() << std::endl;
        }
    });

    sendCommand("Performance.enable", QJsonObject(), [](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            std::cerr << "# CDP Warning: Failed to enable Performance domain: " << error.toStdString() << std::endl;
        }
    });
}

void CDPClient::sendCommand(const QString& method, const QJsonObject& params, ResponseCallback callback)
{
    if (!m_isConnected && !m_isConnecting) {
        callback(QJsonObject(), QString("Not connected to Chrome DevTools. Ensure Tau5 is running with --remote-debugging-port=%1").arg(m_devToolsPort));
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
    getDocument(QJsonObject(), callback);
}

void CDPClient::getDocument(const QJsonObject& options, ResponseCallback callback)
{
    // Default depth of 5 - enough to see actual content without excessive nesting
    int depth = options.value("depth").toInt(5);
    sendCommand("DOM.getDocument", QJsonObject{{"depth", depth}, {"pierce", true}}, callback);
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
    // NOTE: Ignore since_last_call when search/filter parameters are provided
    // because searches should always query the full message history
    bool hasSearchOrFilter = filters.contains("search") || filters.contains("regex") ||
                            filters.contains("level") || filters.contains("since") ||
                            filters.contains("last");
    bool sinceLastCall = filters.value("since_last_call").toBool() && !hasSearchOrFilter;
    if (sinceLastCall && m_lastMessageRetrievalTime.isValid()) {
        sinceTime = m_lastMessageRetrievalTime;
    }

    // Output format
    QString format = filters.value("format").toString("json");

    // Limit (default to 100, -1 for no limit)
    int limit = filters.value("limit").toInt(100);

    // Filter and build messages (iterate in reverse order for newest first)
    for (auto it = m_consoleMessages.rbegin(); it != m_consoleMessages.rend(); ++it) {
        const ConsoleMessage& msg = *it;
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
    m_lastMessageRetrievalTime = QDateTime(); // Reset to invalid to fix since_last_call behavior
}

void CDPClient::markMessageRetrievalTime()
{
    m_lastMessageRetrievalTime = QDateTime::currentDateTime();
}

void CDPClient::navigateTo(const QString& url, ResponseCallback callback)
{
    // Check if this is an absolute URL (includes protocol)
    bool isAbsoluteUrl = url.startsWith("http://") || url.startsWith("https://") || url.startsWith("file://");

    // For relative URLs and localhost URLs, block /dev/* paths
    if (!isAbsoluteUrl || url.contains("://localhost") || url.contains("://127.0.0.1")) {
        // Block navigation to /dev/* paths for local navigation
        if (url.startsWith("/dev/") || url.contains("/dev/dashboard")) {
            callback(QJsonObject{
                {"error", true},
                {"message", "Navigation to /dev/* paths is blocked in Spectra. These are internal debug pages."}
            }, "Navigation blocked: /dev/* paths are not accessible via Spectra");
            return;
        }
    }

    // For Tau5, treat all non-absolute URLs as relative to the current context
    // This works regardless of the actual port being used
    if (!isAbsoluteUrl) {
        // Get current page URL first to resolve relative URLs properly
        evaluateJavaScript("window.location.href", [this, url, callback](const QJsonObject& result, const QString& error) {
            if (!error.isEmpty()) {
                callback(QJsonObject(), "Failed to get current URL: " + error);
                return;
            }

            QString currentUrl = result["result"].toObject()["value"].toString();
            if (currentUrl.isEmpty()) {
                callback(QJsonObject(), "Failed to get current URL");
                return;
            }

            // Check if the resolved URL would go to /dev/*
            QUrl baseUrl(currentUrl);
            QUrl relativeUrl(url);
            QUrl absoluteUrl = baseUrl.resolved(relativeUrl);

            // Only block /dev/* for localhost navigation
            if ((absoluteUrl.host() == "localhost" || absoluteUrl.host() == "127.0.0.1" || absoluteUrl.host().isEmpty())
                && absoluteUrl.path().startsWith("/dev/")) {
                callback(QJsonObject{
                    {"error", true},
                    {"message", "Navigation to /dev/* paths is blocked in Spectra"}
                }, "Navigation blocked: /dev/* paths are not accessible via Spectra");
                return;
            }

            // Navigate to the resolved URL
            QJsonObject params{{"url", absoluteUrl.toString()}};
            sendCommand("Page.navigate", params, [absoluteUrl, callback](const QJsonObject& result, const QString& error) {
                if (!error.isEmpty()) {
                    callback(result, error);
                } else {
                    // Add the resolved URL to the result for clarity
                    QJsonObject enrichedResult = result;
                    enrichedResult["resolvedUrl"] = absoluteUrl.toString();
                    callback(enrichedResult, QString());
                }
            });
        });
    } else {
        // URL is absolute
        QUrl absoluteUrl(url);

        // Only block /dev/* for localhost URLs
        if ((absoluteUrl.host() == "localhost" || absoluteUrl.host() == "127.0.0.1")
            && absoluteUrl.path().startsWith("/dev/")) {
            callback(QJsonObject{
                {"error", true},
                {"message", "Navigation to localhost /dev/* paths is blocked in Spectra"}
            }, "Navigation blocked: /dev/* paths are not accessible via Spectra");
            return;
        }

        // Allow navigation to external URLs (when local-only=false)
        // This enables testing external sites when Tau5 is in dev mode
        QJsonObject params{{"url", url}};
        sendCommand("Page.navigate", params, [url, absoluteUrl, callback](const QJsonObject& result, const QString& error) {
            if (!error.isEmpty()) {
                callback(result, error);
            } else {
                // Add info about external navigation
                QJsonObject enrichedResult = result;
                if (absoluteUrl.host() != "localhost" && absoluteUrl.host() != "127.0.0.1" && !absoluteUrl.host().isEmpty()) {
                    enrichedResult["externalNavigation"] = true;
                    enrichedResult["navigatedTo"] = url;
                }
                callback(enrichedResult, QString());
            }
        });
    }
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

// Network event handling
void CDPClient::handleNetworkEvent(const QString& method, const QJsonObject& params)
{
    if (method == "Network.requestWillBeSent") {
        NetworkRequest request;
        request.requestId = params["requestId"].toString();
        request.timestamp = QDateTime::currentDateTime();

        QJsonObject requestData = params["request"].toObject();
        request.url = requestData["url"].toString();
        request.method = requestData["method"].toString();
        request.headers = requestData["headers"].toObject();

        request.resourceType = params["type"].toString();

        m_networkRequests.append(request);

        // Keep only recent requests
        while (m_networkRequests.size() > MAX_NETWORK_REQUESTS) {
            m_networkRequests.removeFirst();
        }
    } else if (method == "Network.responseReceived") {
        QString requestId = params["requestId"].toString();
        QJsonObject response = params["response"].toObject();

        // Find and update the corresponding request
        for (int i = 0; i < m_networkRequests.size(); i++) {
            if (m_networkRequests[i].requestId == requestId) {
                m_networkRequests[i].statusCode = response["status"].toInt();
                m_networkRequests[i].statusText = response["statusText"].toString();
                m_networkRequests[i].responseHeaders = response["headers"].toObject();
                m_networkRequests[i].mimeType = response["mimeType"].toString();
                m_networkRequests[i].fromCache = response["fromCache"].toBool();

                // Check for CORS headers relevant to WASM
                QJsonObject headers = response["headers"].toObject();
                QString coop = headers["cross-origin-opener-policy"].toString();
                QString coep = headers["cross-origin-embedder-policy"].toString();

                if (!coop.isEmpty() || !coep.isEmpty()) {
                    std::cerr << "# CDP: CORS headers for " << m_networkRequests[i].url.toStdString()
                             << " - COOP: " << coop.toStdString()
                             << ", COEP: " << coep.toStdString() << std::endl;
                }
                break;
            }
        }
    } else if (method == "Network.loadingFinished") {
        QString requestId = params["requestId"].toString();
        qint64 encodedDataLength = params["encodedDataLength"].toDouble();

        for (int i = 0; i < m_networkRequests.size(); i++) {
            if (m_networkRequests[i].requestId == requestId) {
                m_networkRequests[i].encodedDataLength = encodedDataLength;
                break;
            }
        }
    } else if (method == "Network.loadingFailed") {
        QString requestId = params["requestId"].toString();
        QString errorText = params["errorText"].toString();

        for (int i = 0; i < m_networkRequests.size(); i++) {
            if (m_networkRequests[i].requestId == requestId) {
                m_networkRequests[i].failureReason = errorText;
                std::cerr << "# CDP: Network request failed - " << m_networkRequests[i].url.toStdString()
                         << " - Error: " << errorText.toStdString() << std::endl;
                break;
            }
        }
    }
}

// Runtime exception handling
void CDPClient::handleRuntimeException(const QString& method, const QJsonObject& params)
{
    if (method == "Runtime.exceptionThrown") {
        RuntimeException exception;
        exception.timestamp = QDateTime::fromMSecsSinceEpoch(params["timestamp"].toDouble());

        QJsonObject exceptionDetails = params["exceptionDetails"].toObject();
        exception.exceptionId = QString::number(exceptionDetails["exceptionId"].toInt());
        exception.text = exceptionDetails["text"].toString();
        exception.lineNumber = exceptionDetails["lineNumber"].toInt();
        exception.columnNumber = exceptionDetails["columnNumber"].toInt();
        exception.url = exceptionDetails["url"].toString();

        if (exceptionDetails.contains("stackTrace")) {
            exception.stackTrace = exceptionDetails["stackTrace"].toObject();
        }

        if (exceptionDetails.contains("exception")) {
            QJsonObject exceptionObj = exceptionDetails["exception"].toObject();
            exception.exceptionDetails = exceptionObj["description"].toString();
        }

        m_exceptions.append(exception);

        std::cerr << "# CDP: Runtime exception - " << exception.text.toStdString()
                 << " at " << exception.url.toStdString()
                 << ":" << exception.lineNumber << std::endl;
    }
}

// Network monitoring
void CDPClient::getNetworkRequests(const QJsonObject& filters, ResponseCallback callback)
{
    QJsonArray requests;

    QString urlPattern = filters.value("urlPattern").toString();
    bool includeResponse = filters.value("includeResponse").toBool();
    bool includeTimings = filters.value("includeTimings").toBool();
    int limit = filters.value("limit").toInt(100);  // Default 100 to match other tools, -1 for no limit
    int count = 0;

    for (const NetworkRequest& req : m_networkRequests) {
        // Apply URL pattern filter if specified
        if (!urlPattern.isEmpty()) {
            QRegularExpression regex(urlPattern);
            if (!regex.match(req.url).hasMatch()) {
                continue;
            }
        }

        QJsonObject reqObj;
        reqObj["requestId"] = req.requestId;
        reqObj["url"] = req.url;
        reqObj["method"] = req.method;
        reqObj["timestamp"] = req.timestamp.toString(Qt::ISODateWithMs);
        reqObj["resourceType"] = req.resourceType;

        if (includeResponse) {
            reqObj["statusCode"] = req.statusCode;
            reqObj["statusText"] = req.statusText;
            reqObj["mimeType"] = req.mimeType;
            reqObj["responseHeaders"] = req.responseHeaders;
            reqObj["fromCache"] = req.fromCache;

            if (!req.failureReason.isEmpty()) {
                reqObj["failureReason"] = req.failureReason;
            }
        }

        if (includeTimings) {
            reqObj["encodedDataLength"] = static_cast<qint64>(req.encodedDataLength);
        }

        requests.append(reqObj);

        count++;
        if (limit > 0 && count >= limit) {
            break;
        }
    }

    QJsonObject result;
    result["requests"] = requests;
    result["count"] = requests.size();

    callback(result, QString());
}

void CDPClient::clearNetworkRequests()
{
    m_networkRequests.clear();
}

// Performance and Memory
void CDPClient::getMemoryUsage(ResponseCallback callback)
{
    sendCommand("Performance.getMetrics", QJsonObject(), [callback](const QJsonObject& result, const QString& error) {
        if (!error.isEmpty()) {
            callback(QJsonObject(), error);
            return;
        }

        QJsonArray metrics = result["metrics"].toArray();
        QJsonObject memoryInfo;

        for (const QJsonValue& val : metrics) {
            QJsonObject metric = val.toObject();
            QString name = metric["name"].toString();
            double value = metric["value"].toDouble();

            if (name.contains("Memory") || name.contains("JS")) {
                memoryInfo[name] = value;
            }
        }

        callback(memoryInfo, QString());
    });
}

void CDPClient::startProfiling(const QString& profileName, ResponseCallback callback)
{
    QJsonObject params;
    if (!profileName.isEmpty()) {
        params["id"] = profileName;
    }

    sendCommand("Profiler.enable", QJsonObject(), [this, params, callback](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            callback(QJsonObject(), error);
            return;
        }

        sendCommand("Profiler.start", params, callback);
    });
}

void CDPClient::stopProfiling(const QString& profileName, ResponseCallback callback)
{
    QJsonObject params;
    if (!profileName.isEmpty()) {
        params["id"] = profileName;
    }

    sendCommand("Profiler.stop", params, callback);
}

void CDPClient::getHeapSnapshot(ResponseCallback callback)
{
    sendCommand("HeapProfiler.enable", QJsonObject(), [this, callback](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            callback(QJsonObject(), error);
            return;
        }

        // This would normally stream chunks, but for simplicity we'll get a summary
        sendCommand("Runtime.getHeapUsage", QJsonObject(), callback);
    });
}

// Runtime exceptions
void CDPClient::getPendingExceptions(ResponseCallback callback)
{
    QJsonArray exceptions;

    for (const RuntimeException& ex : m_exceptions) {
        QJsonObject exObj;
        exObj["exceptionId"] = ex.exceptionId;
        exObj["text"] = ex.text;
        exObj["url"] = ex.url;
        exObj["lineNumber"] = ex.lineNumber;
        exObj["columnNumber"] = ex.columnNumber;
        exObj["timestamp"] = ex.timestamp.toString(Qt::ISODateWithMs);

        if (!ex.exceptionDetails.isEmpty()) {
            exObj["details"] = ex.exceptionDetails;
        }

        if (!ex.stackTrace.isEmpty()) {
            exObj["stackTrace"] = ex.stackTrace;
        }

        exceptions.append(exObj);
    }

    QJsonObject result;
    result["exceptions"] = exceptions;
    result["count"] = exceptions.size();

    callback(result, QString());
}

void CDPClient::clearExceptions()
{
    m_exceptions.clear();
}

// Resources
void CDPClient::getLoadedResources(ResponseCallback callback)
{
    sendCommand("Page.getResourceTree", QJsonObject(), [callback](const QJsonObject& result, const QString& error) {
        if (!error.isEmpty()) {
            callback(QJsonObject(), error);
            return;
        }

        QJsonArray resources;
        QJsonObject frameTree = result["frameTree"].toObject();

        // Extract resources from frame tree
        std::function<void(const QJsonObject&)> extractResources = [&](const QJsonObject& frame) {
            QJsonArray frameResources = frame["resources"].toArray();
            for (const QJsonValue& val : frameResources) {
                resources.append(val);
            }

            QJsonArray childFrames = frame["childFrames"].toArray();
            for (const QJsonValue& child : childFrames) {
                extractResources(child.toObject());
            }
        };

        extractResources(frameTree);

        QJsonObject finalResult;
        finalResult["resources"] = resources;
        finalResult["count"] = resources.size();

        callback(finalResult, QString());
    });
}

// Audio debugging
void CDPClient::getAudioContexts(ResponseCallback callback)
{
    // Use Runtime.evaluate to get AudioContext information
    QString script = R"(
        (function() {
            const contexts = [];
            if (typeof AudioContext !== 'undefined') {
                // This is a simplified version - in practice we'd need to track contexts
                const ctx = window.__audioContext || null;
                if (ctx) {
                    contexts.push({
                        state: ctx.state,
                        sampleRate: ctx.sampleRate,
                        currentTime: ctx.currentTime,
                        baseLatency: ctx.baseLatency,
                        outputLatency: ctx.outputLatency
                    });
                }
            }
            return contexts;
        })()
    )";

    QJsonObject params;
    params["expression"] = script;
    params["returnByValue"] = true;

    sendCommand("Runtime.evaluate", params, callback);
}

// Workers and Worklets
void CDPClient::getWorkers(ResponseCallback callback)
{
    sendCommand("Target.getTargets", QJsonObject(), [callback](const QJsonObject& result, const QString& error) {
        if (!error.isEmpty()) {
            callback(QJsonObject(), error);
            return;
        }

        QJsonArray targets = result["targetInfos"].toArray();
        QJsonArray workers;

        for (const QJsonValue& val : targets) {
            QJsonObject target = val.toObject();
            QString type = target["type"].toString();

            if (type == "worker" || type == "service_worker" || type == "shared_worker") {
                workers.append(target);
            }
        }

        QJsonObject finalResult;
        finalResult["workers"] = workers;
        finalResult["count"] = workers.size();

        callback(finalResult, QString());
    });
}

// Security
void CDPClient::getSecurityState(ResponseCallback callback)
{
    sendCommand("Security.getSecurityState", QJsonObject(), callback);
}

void CDPClient::getCrossOriginIsolationStatus(ResponseCallback callback)
{
    // Check if SharedArrayBuffer is available and get COOP/COEP status
    QString script = R"(
        (function() {
            return {
                sharedArrayBufferAvailable: typeof SharedArrayBuffer !== 'undefined',
                crossOriginIsolated: self.crossOriginIsolated || false,
                coep: document.featurePolicy ? document.featurePolicy.allowsFeature('cross-origin-isolated') : 'unknown',
                userAgent: navigator.userAgent
            };
        })()
    )";

    QJsonObject params;
    params["expression"] = script;
    params["returnByValue"] = true;

    sendCommand("Runtime.evaluate", params, [callback](const QJsonObject& result, const QString& error) {
        if (!error.isEmpty()) {
            callback(QJsonObject(), error);
            return;
        }

        QJsonObject value = result["result"].toObject()["value"].toObject();
        callback(value, QString());
    });
}

// WASM and AudioWorklet debugging
void CDPClient::getResponseBody(const QString& requestId, ResponseCallback callback)
{
    QJsonObject params;
    params["requestId"] = requestId;

    sendCommand("Network.getResponseBody", params, [callback](const QJsonObject& result, const QString& error) {
        if (!error.isEmpty()) {
            callback(QJsonObject(), error);
            return;
        }

        QJsonObject bodyInfo;
        bodyInfo["body"] = result["body"].toString();
        bodyInfo["base64Encoded"] = result["base64Encoded"].toBool();

        if (result["base64Encoded"].toBool()) {
            QByteArray decoded = QByteArray::fromBase64(result["body"].toString().toUtf8());
            bodyInfo["decodedSize"] = decoded.size();

            // Check if it's a WASM module (magic number: 0x6d736100)
            if (decoded.size() >= 4) {
                uint32_t magic = *reinterpret_cast<const uint32_t*>(decoded.constData());
                if (magic == 0x6d736100) {
                    bodyInfo["isWasmModule"] = true;
                    bodyInfo["wasmVersion"] = decoded.size() >= 8 ?
                        static_cast<int>(*reinterpret_cast<const uint32_t*>(decoded.constData() + 4)) : 0;
                }
            }
        }

        callback(bodyInfo, QString());
    });
}

void CDPClient::getAudioWorkletState(ResponseCallback callback)
{
    // Check AudioWorklet state by evaluating JavaScript
    QString script = R"(
        (function() {
            const result = {
                audioContexts: [],
                workletNodes: [],
                workletProcessors: []
            };

            // Find all AudioContexts
            if (typeof AudioContext !== 'undefined') {
                // Check if we have access to any audio contexts
                const ctx = window.__audioContext || window.audioContext || null;
                if (ctx) {
                    result.audioContexts.push({
                        state: ctx.state,
                        sampleRate: ctx.sampleRate,
                        currentTime: ctx.currentTime,
                        hasWorklet: ctx.audioWorklet !== undefined
                    });

                    // Check if AudioWorklet is available
                    if (ctx.audioWorklet) {
                        result.audioWorkletAvailable = true;
                        // We can't directly query registered processors from main thread
                        // but we can check if common ones exist by trying to create nodes
                    }
                }
            }

            // Check for global AudioWorkletNode references
            if (typeof AudioWorkletNode !== 'undefined') {
                result.audioWorkletNodeAvailable = true;
            }

            // Check for SharedArrayBuffer in AudioWorklet context
            result.sharedArrayBufferAvailable = typeof SharedArrayBuffer !== 'undefined';

            return result;
        })()
    )";

    QJsonObject params;
    params["expression"] = script;
    params["returnByValue"] = true;

    sendCommand("Runtime.evaluate", params, [callback](const QJsonObject& result, const QString& error) {
        if (!error.isEmpty()) {
            callback(QJsonObject(), error);
            return;
        }

        QJsonObject value = result["result"].toObject()["value"].toObject();
        callback(value, QString());
    });
}

void CDPClient::monitorWasmInstantiation(ResponseCallback callback)
{
    // Inject monitoring for WebAssembly.instantiate
    QString script = R"(
        (function() {
            if (typeof WebAssembly === 'undefined') {
                return { available: false };
            }

            // Get currently loaded modules if we've been tracking
            const modules = window.__wasmModules || [];

            // Inject monitoring if not already done
            if (!window.__wasmMonitoringEnabled) {
                window.__wasmModules = [];
                const originalInstantiate = WebAssembly.instantiate;
                const originalInstantiateStreaming = WebAssembly.instantiateStreaming;

                WebAssembly.instantiate = function(...args) {
                    const startTime = performance.now();
                    const promise = originalInstantiate.apply(this, args);

                    promise.then(result => {
                        const info = {
                            timestamp: new Date().toISOString(),
                            method: 'instantiate',
                            success: true,
                            duration: performance.now() - startTime,
                            hasModule: result.module !== undefined,
                            hasInstance: result.instance !== undefined
                        };

                        if (result.instance) {
                            info.exports = Object.keys(result.instance.exports);
                        }

                        window.__wasmModules.push(info);
                        console.log('[WASM] Instantiation successful:', info);
                    }).catch(error => {
                        const info = {
                            timestamp: new Date().toISOString(),
                            method: 'instantiate',
                            success: false,
                            error: error.toString(),
                            duration: performance.now() - startTime
                        };
                        window.__wasmModules.push(info);
                        console.error('[WASM] Instantiation failed:', error);
                    });

                    return promise;
                };

                WebAssembly.instantiateStreaming = function(response, imports) {
                    const startTime = performance.now();

                    // Clone response to get URL
                    response.clone().url && console.log('[WASM] Loading from:', response.url);

                    const promise = originalInstantiateStreaming.call(this, response, imports);

                    promise.then(result => {
                        const info = {
                            timestamp: new Date().toISOString(),
                            method: 'instantiateStreaming',
                            success: true,
                            duration: performance.now() - startTime,
                            hasModule: result.module !== undefined,
                            hasInstance: result.instance !== undefined
                        };

                        if (result.instance) {
                            info.exports = Object.keys(result.instance.exports);
                        }

                        window.__wasmModules.push(info);
                        console.log('[WASM] Streaming instantiation successful:', info);
                    }).catch(error => {
                        const info = {
                            timestamp: new Date().toISOString(),
                            method: 'instantiateStreaming',
                            success: false,
                            error: error.toString(),
                            duration: performance.now() - startTime
                        };
                        window.__wasmModules.push(info);
                        console.error('[WASM] Streaming instantiation failed:', error);
                    });

                    return promise;
                };

                window.__wasmMonitoringEnabled = true;
            }

            return {
                available: true,
                monitoringEnabled: true,
                instantiations: window.__wasmModules
            };
        })()
    )";

    QJsonObject params;
    params["expression"] = script;
    params["returnByValue"] = true;

    sendCommand("Runtime.evaluate", params, [callback](const QJsonObject& result, const QString& error) {
        if (!error.isEmpty()) {
            callback(QJsonObject(), error);
            return;
        }

        QJsonObject value = result["result"].toObject()["value"].toObject();
        callback(value, QString());
    });
}

void CDPClient::getPerformanceTimeline(ResponseCallback callback)
{
    QString script = R"(
        (function() {
            const timeline = {
                navigation: {},
                resources: [],
                measures: [],
                marks: []
            };

            // Navigation timing
            if (performance.timing) {
                const t = performance.timing;
                timeline.navigation = {
                    domContentLoaded: t.domContentLoadedEventEnd - t.navigationStart,
                    loadComplete: t.loadEventEnd - t.navigationStart,
                    domInteractive: t.domInteractive - t.navigationStart
                };
            }

            // Resource timing (focus on WASM and AudioWorklet files)
            if (performance.getEntriesByType) {
                const resources = performance.getEntriesByType('resource');
                timeline.resources = resources
                    .filter(r => r.name.includes('.wasm') ||
                                r.name.includes('audioworklet') ||
                                r.name.includes('worklet'))
                    .map(r => ({
                        name: r.name,
                        duration: r.duration,
                        startTime: r.startTime,
                        transferSize: r.transferSize || 0,
                        decodedBodySize: r.decodedBodySize || 0
                    }));

                // User timing marks and measures
                timeline.marks = performance.getEntriesByType('mark').map(m => ({
                    name: m.name,
                    startTime: m.startTime
                }));

                timeline.measures = performance.getEntriesByType('measure').map(m => ({
                    name: m.name,
                    duration: m.duration,
                    startTime: m.startTime
                }));
            }

            // Memory info if available
            if (performance.memory) {
                timeline.memory = {
                    usedJSHeapSize: performance.memory.usedJSHeapSize,
                    totalJSHeapSize: performance.memory.totalJSHeapSize,
                    jsHeapSizeLimit: performance.memory.jsHeapSizeLimit
                };
            }

            return timeline;
        })()
    )";

    QJsonObject params;
    params["expression"] = script;
    params["returnByValue"] = true;

    sendCommand("Runtime.evaluate", params, callback);
}

void CDPClient::executeInAudioWorklet(const QString& code, ResponseCallback callback)
{
    // First check if AudioWorklet is available
    QString checkScript = R"(
        (function() {
            const ctx = window.__audioContext || window.audioContext || null;
            if (!ctx || !ctx.audioWorklet) {
                return { error: 'No AudioContext with audioWorklet found' };
            }
            return { hasWorklet: true };
        })()
    )";

    QJsonObject checkParams;
    checkParams["expression"] = checkScript;
    checkParams["returnByValue"] = true;

    sendCommand("Runtime.evaluate", checkParams, [this, code, callback](const QJsonObject& result, const QString& error) {
        if (!error.isEmpty()) {
            callback(QJsonObject(), error);
            return;
        }

        QJsonObject checkResult = result["result"].toObject()["value"].toObject();
        if (checkResult.contains("error")) {
            callback(checkResult, QString());
            return;
        }

        // AudioWorklet is available, but we can't directly execute in its context from here
        // We need to use messaging or create a special processor
        QString executeScript = QString(R"(
            (function() {
                // This is a limitation - we can't directly execute in AudioWorklet context
                // We would need to create a special debugging processor
                return {
                    limitation: 'Cannot directly execute in AudioWorklet context from DevTools',
                    suggestion: 'Use console.log in AudioWorkletProcessor or create debug processor',
                    providedCode: %1
                };
            })()
        )").arg(QJsonDocument(QJsonValue(code).toObject()).toJson());

        QJsonObject execParams;
        execParams["expression"] = executeScript;
        execParams["returnByValue"] = true;

        sendCommand("Runtime.evaluate", execParams, callback);
    });
}

// LiveView debugging implementation
void CDPClient::handleWebSocketEvent(const QString& method, const QJsonObject& params)
{
    if (method == "Network.webSocketFrameReceived" || method == "Network.webSocketFrameSent") {
        WebSocketFrame frame;
        frame.timestamp = QDateTime::fromMSecsSinceEpoch(params["timestamp"].toDouble());
        frame.requestId = params["requestId"].toString();

        QJsonObject response = params["response"].toObject();
        frame.opcode = response["opcode"].toString();
        frame.payloadData = response["payloadData"].toString();
        frame.sent = (method == "Network.webSocketFrameSent");

        // Get URL from associated request
        for (const auto& req : m_networkRequests) {
            if (req.requestId == frame.requestId) {
                frame.url = req.url;
                break;
            }
        }

        m_webSocketFrames.append(frame);

        // Limit storage
        while (m_webSocketFrames.size() > MAX_WEBSOCKET_FRAMES) {
            m_webSocketFrames.removeFirst();
        }
    }
}

void CDPClient::getWebSocketFrames(const QJsonObject& filters, ResponseCallback callback)
{
    QJsonArray frames;

    QString urlFilter = filters["url"].toString();
    bool sentOnly = filters["sentOnly"].toBool(false);
    bool receivedOnly = filters["receivedOnly"].toBool(false);
    QString searchText = filters["search"].toString();
    int limit = filters["limit"].toInt(100);  // Default 100, -1 for no limit

    int count = 0;
    for (const auto& frame : m_webSocketFrames) {
        // Apply filters
        if (!urlFilter.isEmpty() && !frame.url.contains(urlFilter))
            continue;
        if (sentOnly && !frame.sent)
            continue;
        if (receivedOnly && frame.sent)
            continue;
        if (!searchText.isEmpty() && !frame.payloadData.contains(searchText, Qt::CaseInsensitive))
            continue;

        QJsonObject frameObj;
        frameObj["timestamp"] = frame.timestamp.toString(Qt::ISODate);
        frameObj["direction"] = frame.sent ? "sent" : "received";
        frameObj["opcode"] = frame.opcode;
        frameObj["url"] = frame.url;

        // Parse LiveView messages if they're JSON
        if (frame.opcode == "text" && frame.payloadData.startsWith("{") || frame.payloadData.startsWith("[")) {
            QJsonDocument doc = QJsonDocument::fromJson(frame.payloadData.toUtf8());
            if (!doc.isNull()) {
                frameObj["parsedData"] = doc.object().isEmpty() ? QJsonValue(doc.array()) : QJsonValue(doc.object());

                // Detect LiveView message types
                QJsonArray messages = doc.array();
                if (!messages.isEmpty()) {
                    QJsonObject firstMsg = messages[0].toObject();
                    QString event = firstMsg["event"].toString();
                    if (!event.isEmpty()) {
                        frameObj["liveViewEvent"] = event;
                    }
                }
            } else {
                frameObj["data"] = frame.payloadData;
            }
        } else {
            frameObj["data"] = frame.payloadData;
        }

        frames.append(frameObj);

        count++;
        if (limit > 0 && count >= limit)
            break;
    }

    QJsonObject result;
    result["frames"] = frames;
    result["total"] = m_webSocketFrames.size();

    callback(result, QString());
}

void CDPClient::clearWebSocketFrames()
{
    m_webSocketFrames.clear();
}

void CDPClient::startDOMMutationObserver(const QString& selector, ResponseCallback callback)
{
    QString observerScript = QString(R"(
        (function() {
            if (window.__cdpMutationObserver) {
                window.__cdpMutationObserver.disconnect();
            }

            const targetNode = document.querySelector('%1');
            if (!targetNode) {
                return { error: 'Element not found: %1' };
            }

            window.__cdpMutationObserver = new MutationObserver(function(mutations) {
                mutations.forEach(function(mutation) {
                    console.log('[DOM_MUTATION]', JSON.stringify({
                        type: mutation.type,
                        target: mutation.target.tagName || mutation.target.nodeType,
                        attributeName: mutation.attributeName,
                        oldValue: mutation.oldValue,
                        addedNodes: Array.from(mutation.addedNodes).map(n => n.tagName || n.nodeType),
                        removedNodes: Array.from(mutation.removedNodes).map(n => n.tagName || n.nodeType)
                    }));
                });
            });

            window.__cdpMutationObserver.observe(targetNode, {
                attributes: true,
                attributeOldValue: true,
                characterData: true,
                characterDataOldValue: true,
                childList: true,
                subtree: true
            });

            return { success: true, observing: '%1' };
        })();
    )").arg(selector);

    evaluateJavaScript(observerScript, callback);
}

void CDPClient::stopDOMMutationObserver(ResponseCallback callback)
{
    QString script = R"(
        (function() {
            if (window.__cdpMutationObserver) {
                window.__cdpMutationObserver.disconnect();
                delete window.__cdpMutationObserver;
                return { success: true };
            }
            return { success: false, error: 'No observer running' };
        })();
    )";

    evaluateJavaScript(script, callback);
}

void CDPClient::getDOMMutations(ResponseCallback callback)
{
    getDOMMutations(QJsonObject(), callback);
}

void CDPClient::getDOMMutations(const QJsonObject& options, ResponseCallback callback)
{
    // Parse mutations from console messages
    QJsonArray mutations;
    int limit = options.value("limit").toInt(100);  // Default 100, but allow override
    int count = 0;

    for (const auto& msg : m_consoleMessages) {
        if (msg.text.startsWith("[DOM_MUTATION]")) {
            QString jsonStr = msg.text.mid(14).trimmed();
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
            if (!doc.isNull()) {
                QJsonObject mutation = doc.object();
                mutation["timestamp"] = msg.timestamp.toString(Qt::ISODate);
                mutations.append(mutation);

                count++;
                if (limit > 0 && count >= limit) {
                    break;
                }
            }
        }
    }

    QJsonObject result;
    result["mutations"] = mutations;
    result["count"] = mutations.size();

    callback(result, QString());
}

void CDPClient::clearDOMMutations()
{
    // Remove DOM mutation messages from console
    m_consoleMessages.erase(
        std::remove_if(m_consoleMessages.begin(), m_consoleMessages.end(),
                      [](const ConsoleMessage& msg) {
                          return msg.text.startsWith("[DOM_MUTATION]");
                      }),
        m_consoleMessages.end());
}

void CDPClient::getJavaScriptProfile(ResponseCallback callback)
{
    // Get performance metrics for LiveView hooks
    QString profileScript = R"(
        (function() {
            const entries = performance.getEntriesByType('measure')
                .filter(e => e.name.includes('hook') || e.name.includes('LiveView'))
                .map(e => ({
                    name: e.name,
                    duration: e.duration,
                    startTime: e.startTime
                }));

            // Check for long tasks
            const longTasks = [];
            if (window.PerformanceObserver && PerformanceObserver.supportedEntryTypes.includes('longtask')) {
                // Would need to set up observer beforehand
            }

            // Get hook execution stats if we're tracking them
            const hookStats = window.__liveViewHookStats || {};

            return {
                measures: entries,
                hookStats: hookStats,
                totalJSHeapSize: performance.memory ? performance.memory.totalJSHeapSize : null,
                usedJSHeapSize: performance.memory ? performance.memory.usedJSHeapSize : null
            };
        })();
    )";

    evaluateJavaScript(profileScript, callback);
}

void CDPClient::trackLiveViewEvent(const QString& eventType, const QJsonObject& details)
{
    // This would be called by the MCP server when specific LiveView events are detected
    // For now, just log it
    QString message = QString("LiveView Event: %1").arg(eventType);
    emit logMessage(message);
}

// Target selection methods
QJsonArray CDPClient::getAvailableTargets()
{
    // Fetch fresh targets list
    QUrl url(QString("http://localhost:%1/json/list").arg(m_devToolsPort));
    QNetworkRequest request(url);

    QNetworkReply* reply = m_networkManager->get(request);

    // Wait synchronously for the reply (acceptable for MCP tool calls)
    while (!reply->isFinished()) {
        QCoreApplication::processEvents();
    }

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isArray()) {
            m_lastTargetList = doc.array();
        }
    }
    reply->deleteLater();

    return m_lastTargetList;
}

bool CDPClient::setTargetByTitle(const QString& title)
{
    // Store the new target title
    m_targetTitle = title;

    // If we're connected, disconnect and reconnect to the new target
    if (m_isConnected) {
        std::cerr << "# CDP: Switching target to '" << title.toStdString() << "'" << std::endl;
        disconnect();

        // Reconnect with new target
        return connect();
    } else {
        // Not connected, just update target for next connection
        std::cerr << "# CDP: Target set to '" << title.toStdString() << "' for next connection" << std::endl;
        return true;
    }
}