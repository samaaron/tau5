#ifndef CDPCLIENT_H
#define CDPCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QMap>
#include <QTimer>
#include <functional>
#include <memory>

class CDPClient : public QObject
{
    Q_OBJECT

public:
    enum class ConnectionState {
        NotConnected,
        Connecting,
        Connected,
        Failed
    };

    using ResponseCallback = std::function<void(const QJsonObject& result, const QString& error)>;

    explicit CDPClient(quint16 devToolsPort, QObject* parent = nullptr);
    ~CDPClient();

    bool connect();
    void disconnect();
    bool isConnected() const;
    ConnectionState getConnectionState() const;

    void sendCommand(const QString& method, const QJsonObject& params, ResponseCallback callback);
    
    void getDocument(ResponseCallback callback);
    void querySelector(const QString& selector, ResponseCallback callback);
    void getOuterHTML(int nodeId, ResponseCallback callback);
    void evaluateJavaScript(const QString& expression, ResponseCallback callback);
    void evaluateJavaScriptWithObjectReferences(const QString& expression, ResponseCallback callback);
    void getConsoleMessages(ResponseCallback callback);
    void navigateTo(const QString& url, ResponseCallback callback);
    
    void setAttributeValue(int nodeId, const QString& name, const QString& value, ResponseCallback callback);
    void removeAttribute(int nodeId, const QString& name, ResponseCallback callback);
    void setOuterHTML(int nodeId, const QString& html, ResponseCallback callback);
    
    // Object reference methods
    void getProperties(const QString& objectId, ResponseCallback callback);
    void callFunctionOn(const QString& objectId, const QString& functionDeclaration, ResponseCallback callback);
    void releaseObject(const QString& objectId, ResponseCallback callback);

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorMessage);
    void consoleMessage(const QString& level, const QString& text);
    void domContentUpdated();
    void logMessage(const QString& message);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onPingTimeout();

private:
    void sendRawCommand(const QJsonObject& command);
    void processResponse(const QJsonObject& response);
    void enableDomains();
    
    void fetchTargetList();
    QString findMainPageTarget(const QJsonArray& targets);
    void connectToTarget(const QString& targetId);
    void discoverTargets();

private:
    quint16 m_devToolsPort;
    QWebSocket* m_webSocket;
    QNetworkAccessManager* m_networkManager;
    QTimer* m_pingTimer;
    
    int m_nextCommandId;
    QMap<int, ResponseCallback> m_pendingCommands;
    
    QString m_targetId;
    QString m_webSocketDebuggerUrl;
    
    bool m_isConnecting;
    bool m_isConnected;
    ConnectionState m_connectionState;
    
    static constexpr int PING_INTERVAL_MS = 30000;
    static constexpr int CONNECTION_TIMEOUT_MS = 5000;
};

#endif // CDPCLIENT_H