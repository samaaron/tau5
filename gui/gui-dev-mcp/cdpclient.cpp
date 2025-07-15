#include "cdpclient.h"
#include "../logger.h"
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
    Logger::log(Logger::Info, QString("Connecting to Chrome DevTools Protocol on port %1").arg(m_devToolsPort));
    
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
    m_webSocketDebuggerUrl.clear();
    m_targetId.clear();
}

bool CDPClient::isConnected() const
{
    return m_isConnected;
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
            Logger::log(Logger::Debug, errorMsg);
            m_isConnecting = false;
            m_isConnected = false;
            if (m_webSocket->state() != QAbstractSocket::UnconnectedState) {
                m_webSocket->abort();
            }
            emit connectionFailed(errorMsg);
            return;
        }
        
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) {
            QString errorMsg = "Invalid DevTools target list format - Tau5 may not be running";
            Logger::log(Logger::Debug, errorMsg);
            m_isConnecting = false;
            m_isConnected = false;
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
            Logger::log(Logger::Debug, errorMsg);
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
            Logger::log(Logger::Debug, errorMsg);
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
            Logger::log(Logger::Debug, QString("Found main page target: %1").arg(target["title"].toString()));
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
    
    Logger::log(Logger::Info, QString("Connecting to DevTools WebSocket: %1").arg(m_webSocketDebuggerUrl));
    
    m_webSocket->open(QUrl(m_webSocketDebuggerUrl));
}

void CDPClient::onConnected()
{
    Logger::log(Logger::Info, "Connected to Chrome DevTools Protocol");
    m_isConnected = true;
    m_isConnecting = false;
    m_pingTimer->start();
    
    enableDomains();
    
    emit connected();
    emit logMessage("CDP Client connected");
}

void CDPClient::onDisconnected()
{
    Logger::log(Logger::Info, "Disconnected from Chrome DevTools Protocol");
    m_isConnected = false;
    m_isConnecting = false;
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
        Logger::log(Logger::Warning, "Received invalid CDP message");
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
            QString text;
            for (const QJsonValue& arg : args) {
                QJsonObject argObj = arg.toObject();
                if (argObj["type"].toString() == "string") {
                    text += argObj["value"].toString() + " ";
                }
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
            Logger::log(Logger::Warning, QString("Failed to enable DOM domain: %1").arg(error));
        }
    });
    
    sendCommand("Runtime.enable", QJsonObject(), [](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            Logger::log(Logger::Warning, QString("Failed to enable Runtime domain: %1").arg(error));
        }
    });
    
    sendCommand("Page.enable", QJsonObject(), [](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) {
            Logger::log(Logger::Warning, QString("Failed to enable Page domain: %1").arg(error));
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
    
    Logger::log(Logger::Debug, QString("CDP -> %1").arg(message));
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

void CDPClient::getConsoleMessages(ResponseCallback callback)
{
    callback(QJsonObject{{"messages", QJsonArray()}}, QString());
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

void CDPClient::discoverTargets()
{
    fetchTargetList();
}