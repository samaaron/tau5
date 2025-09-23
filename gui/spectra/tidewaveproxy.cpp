#include "tidewaveproxy.h"
#include <QNetworkRequest>
#include <QMutexLocker>
#include <QDebug>

TidewaveProxy::TidewaveProxy(quint16 tidewavePort, QObject* parent)
    : QObject(parent)
    , m_port(tidewavePort)
    , m_baseUrl(QString("http://localhost:%1/tidewave/mcp").arg(tidewavePort))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_healthCheckTimer(new QTimer(this))
    , m_nextRequestId(1)
    , m_available(false)
    , m_initialized(false)
{
    connect(m_healthCheckTimer, &QTimer::timeout, this, &TidewaveProxy::checkHealth);
    m_healthCheckTimer->setInterval(HEALTH_CHECK_INTERVAL_MS);

    // Initial availability check
    checkAvailability();
}

TidewaveProxy::~TidewaveProxy()
{
    m_healthCheckTimer->stop();

    // Cancel all pending network requests
    for (auto reply : m_replyToRequestId.keys()) {
        reply->abort();
        reply->deleteLater();
    }
}

bool TidewaveProxy::isAvailable() const
{
    return m_available;
}

void TidewaveProxy::checkAvailability()
{
    // Simple ping to check if Tidewave is available
    QJsonObject pingRequest = createJsonRpcRequest("ping", {});

    QNetworkRequest netRequest(m_baseUrl);
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setRawHeader("User-Agent", "Tau5-Spectra-TidewaveProxy/1.0");

    QByteArray jsonData = QJsonDocument(pingRequest).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_networkManager->post(netRequest, jsonData);

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        bool wasAvailable = m_available;
        m_available = (reply->error() == QNetworkReply::NoError);

        if (m_available != wasAvailable) {
            emit availabilityChanged(m_available);
            emit logMessage(QString("Tidewave proxy availability changed: %1")
                          .arg(m_available ? "available" : "unavailable"));
        }

        if (m_available && !m_healthCheckTimer->isActive()) {
            m_healthCheckTimer->start();
        } else if (!m_available && m_healthCheckTimer->isActive()) {
            m_healthCheckTimer->stop();
        }

        reply->deleteLater();
    });
}

void TidewaveProxy::checkHealth()
{
    checkAvailability();
}

QJsonObject TidewaveProxy::createJsonRpcRequest(const QString& method, const QJsonObject& params)
{
    QMutexLocker locker(&m_requestMutex);

    QJsonObject request;
    request["jsonrpc"] = JSONRPC_VERSION;
    request["id"] = m_nextRequestId++;
    request["method"] = method;

    if (!params.isEmpty()) {
        request["params"] = params;
    }

    return request;
}

void TidewaveProxy::sendRequest(const QJsonObject& request, ResponseCallback callback)
{
    QNetworkRequest netRequest(m_baseUrl);
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setRawHeader("User-Agent", "Tau5-Spectra-TidewaveProxy/1.0");

    QByteArray jsonData = QJsonDocument(request).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = m_networkManager->post(netRequest, jsonData);

    int requestId = request["id"].toInt();

    {
        QMutexLocker locker(&m_requestMutex);
        m_pendingCallbacks[requestId] = callback;
        m_replyToRequestId[reply] = requestId;
    }

    connect(reply, &QNetworkReply::finished, this, &TidewaveProxy::handleNetworkReply);
    connect(reply, &QNetworkReply::errorOccurred, this, [this, reply](QNetworkReply::NetworkError error) {
        Q_UNUSED(error)
        emit logMessage(QString("Tidewave proxy network error: %1").arg(reply->errorString()));
    });
}

void TidewaveProxy::handleNetworkReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    reply->deleteLater();

    ResponseCallback callback;
    int requestId;

    {
        QMutexLocker locker(&m_requestMutex);

        if (!m_replyToRequestId.contains(reply)) {
            return;
        }

        requestId = m_replyToRequestId.take(reply);

        if (!m_pendingCallbacks.contains(requestId)) {
            return;
        }

        callback = m_pendingCallbacks.take(requestId);
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMessage = QString("Network error: %1").arg(reply->errorString());
        callback({}, errorMessage);
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QString errorMessage = QString("JSON parse error: %1").arg(parseError.errorString());
        callback({}, errorMessage);
        return;
    }

    QJsonObject response = doc.object();

    // Check for JSON-RPC error
    if (response.contains("error")) {
        QJsonObject error = response["error"].toObject();
        QString errorMessage = error["message"].toString();
        if (error.contains("data")) {
            errorMessage += QString(" - %1").arg(QJsonDocument(error["data"].toObject()).toJson(QJsonDocument::Compact));
        }
        callback({}, errorMessage);
        return;
    }

    // Extract result
    if (response.contains("result")) {
        callback(response["result"].toObject(), QString());
    } else {
        callback({}, "No result in response");
    }
}

void TidewaveProxy::initialize(const QJsonObject& params, ResponseCallback callback)
{
    QJsonObject initParams = params;
    if (!initParams.contains("protocolVersion")) {
        initParams["protocolVersion"] = MCP_VERSION;
    }

    QJsonObject request = createJsonRpcRequest("initialize", initParams);
    sendRequest(request, [this, callback](const QJsonObject& result, const QString& error) {
        if (error.isEmpty()) {
            m_initialized = true;
            emit logMessage("Tidewave proxy initialized successfully");
        }
        callback(result, error);
    });
}

void TidewaveProxy::listTools(ResponseCallback callback)
{
    QJsonObject request = createJsonRpcRequest("tools/list", {});
    sendRequest(request, callback);
}

void TidewaveProxy::callTool(const QString& toolName, const QJsonObject& arguments, ResponseCallback callback)
{
    QJsonObject params;
    params["name"] = toolName;
    params["arguments"] = arguments;

    QJsonObject request = createJsonRpcRequest("tools/call", params);
    sendRequest(request, callback);
}