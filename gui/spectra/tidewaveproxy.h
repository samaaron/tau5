#ifndef TIDEWAVEPROXY_H
#define TIDEWAVEPROXY_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>
#include <QTimer>
#include <QMutex>
#include <functional>
#include <memory>

class TidewaveProxy : public QObject
{
    Q_OBJECT

public:
    using ResponseCallback = std::function<void(const QJsonObject& result, const QString& error)>;

    explicit TidewaveProxy(quint16 tidewavePort, QObject* parent = nullptr);
    ~TidewaveProxy();

    bool isAvailable() const;
    void checkAvailability();

    // MCP protocol methods
    void initialize(const QJsonObject& params, ResponseCallback callback);
    void listTools(ResponseCallback callback);
    void callTool(const QString& toolName, const QJsonObject& arguments, ResponseCallback callback);

signals:
    void availabilityChanged(bool available);
    void logMessage(const QString& message);

private slots:
    void handleNetworkReply();
    void checkHealth();

private:
    void sendRequest(const QJsonObject& request, ResponseCallback callback);
    QJsonObject createJsonRpcRequest(const QString& method, const QJsonObject& params);

private:
    quint16 m_port;
    QUrl m_baseUrl;
    QNetworkAccessManager* m_networkManager;
    QTimer* m_healthCheckTimer;

    int m_nextRequestId;
    QMap<int, ResponseCallback> m_pendingCallbacks;
    QMap<QNetworkReply*, int> m_replyToRequestId;
    QMutex m_requestMutex;  // Protects request ID generation and maps

    bool m_available;
    bool m_initialized;

    static constexpr const char* JSONRPC_VERSION = "2.0";
    static constexpr const char* MCP_VERSION = "2025-03-26";
    static constexpr int HEALTH_CHECK_INTERVAL_MS = 5000;
};

#endif // TIDEWAVEPROXY_H