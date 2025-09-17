#ifndef CDPCLIENT_H
#define CDPCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QMap>
#include <QTimer>
#include <QList>
#include <QDateTime>
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
    void getDocument(const QJsonObject& options, ResponseCallback callback);
    void querySelector(const QString& selector, ResponseCallback callback);
    void getOuterHTML(int nodeId, ResponseCallback callback);
    void evaluateJavaScript(const QString& expression, ResponseCallback callback);
    void evaluateJavaScriptWithObjectReferences(const QString& expression, ResponseCallback callback);
    void getConsoleMessages(const QJsonObject& filters, ResponseCallback callback);
    void clearConsoleMessages();
    void markMessageRetrievalTime();
    void navigateTo(const QString& url, ResponseCallback callback);
    
    void setAttributeValue(int nodeId, const QString& name, const QString& value, ResponseCallback callback);
    void removeAttribute(int nodeId, const QString& name, ResponseCallback callback);
    void setOuterHTML(int nodeId, const QString& html, ResponseCallback callback);
    
    // Object reference methods
    void getProperties(const QString& objectId, ResponseCallback callback);
    void callFunctionOn(const QString& objectId, const QString& functionDeclaration, ResponseCallback callback);
    void releaseObject(const QString& objectId, ResponseCallback callback);

    // Network monitoring
    void getNetworkRequests(const QJsonObject& filters, ResponseCallback callback);
    void clearNetworkRequests();

    // Performance and Memory
    void getMemoryUsage(ResponseCallback callback);
    void startProfiling(const QString& profileName, ResponseCallback callback);
    void stopProfiling(const QString& profileName, ResponseCallback callback);
    void getHeapSnapshot(ResponseCallback callback);

    // Runtime exceptions
    void getPendingExceptions(ResponseCallback callback);
    void clearExceptions();

    // Resources
    void getLoadedResources(ResponseCallback callback);

    // Audio debugging
    void getAudioContexts(ResponseCallback callback);

    // Workers and Worklets
    void getWorkers(ResponseCallback callback);

    // Security
    void getSecurityState(ResponseCallback callback);
    void getCrossOriginIsolationStatus(ResponseCallback callback);

    // WASM and AudioWorklet debugging
    void getResponseBody(const QString& requestId, ResponseCallback callback);
    void getAudioWorkletState(ResponseCallback callback);
    void monitorWasmInstantiation(ResponseCallback callback);
    void getPerformanceTimeline(ResponseCallback callback);
    void executeInAudioWorklet(const QString& code, ResponseCallback callback);

    // LiveView debugging
    void getWebSocketFrames(const QJsonObject& filters, ResponseCallback callback);
    void clearWebSocketFrames();
    void startDOMMutationObserver(const QString& selector, ResponseCallback callback);
    void stopDOMMutationObserver(ResponseCallback callback);
    void getDOMMutations(ResponseCallback callback);
    void getDOMMutations(const QJsonObject& options, ResponseCallback callback);
    void clearDOMMutations();
    void getJavaScriptProfile(ResponseCallback callback);
    void trackLiveViewEvent(const QString& eventType, const QJsonObject& details);

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

    void handleNetworkEvent(const QString& method, const QJsonObject& params);
    void handleRuntimeException(const QString& method, const QJsonObject& params);
    void handleWebSocketEvent(const QString& method, const QJsonObject& params);

private:
    struct ConsoleMessage {
        QDateTime timestamp;
        QString level;
        QString text;
        QString stackTrace;
        QString url;           // Source URL
        int lineNumber;        // Line number in source
        int columnNumber;      // Column number in source
        QString functionName;  // Function name if available
        QJsonArray args;       // Preserve structured arguments
        QString groupId;       // For console.group support
        bool isGroupStart;     // console.group
        bool isGroupEnd;       // console.groupEnd
    };

    quint16 m_devToolsPort;
    QList<ConsoleMessage> m_consoleMessages;
    static const int MAX_CONSOLE_MESSAGES = 1000;
    QDateTime m_lastMessageRetrievalTime;
    QMap<QString, qint64> m_performanceTimers;  // For console.time tracking

    // Network monitoring
    struct NetworkRequest {
        QString requestId;
        QString url;
        QString method;
        QDateTime timestamp;
        QJsonObject headers;
        QString resourceType;
        int statusCode;
        QString statusText;
        QJsonObject responseHeaders;
        qint64 responseSize;
        qint64 encodedDataLength;
        double timing;
        QString mimeType;
        QString failureReason;
        bool fromCache;
    };
    QList<NetworkRequest> m_networkRequests;
    static const int MAX_NETWORK_REQUESTS = 500;

    // Runtime exceptions
    struct RuntimeException {
        QString exceptionId;
        QString text;
        int lineNumber;
        int columnNumber;
        QString url;
        QJsonObject stackTrace;
        QDateTime timestamp;
        QString exceptionDetails;
    };
    QList<RuntimeException> m_exceptions;

    // WASM instantiation tracking
    struct WasmInstantiation {
        QString url;
        QDateTime timestamp;
        bool success;
        QString errorMessage;
        qint64 moduleSize;
        QJsonObject imports;
        QJsonObject exports;
    };
    QList<WasmInstantiation> m_wasmInstantiations;

    // WebSocket frame tracking for LiveView
    struct WebSocketFrame {
        QDateTime timestamp;
        QString requestId;
        QString opcode;  // text, binary, close, ping, pong
        QString payloadData;
        bool sent;  // true if sent, false if received
        QString url;
    };
    QList<WebSocketFrame> m_webSocketFrames;
    static const int MAX_WEBSOCKET_FRAMES = 200;

    // DOM mutations for LiveView morphdom tracking
    struct DOMMutation {
        QDateTime timestamp;
        QString type;  // childList, attributes, characterData
        int nodeId;
        QString nodeName;
        QString attributeName;
        QString oldValue;
        QString newValue;
        QJsonArray addedNodes;
        QJsonArray removedNodes;
    };
    QList<DOMMutation> m_domMutations;
    static const int MAX_DOM_MUTATIONS = 100;

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