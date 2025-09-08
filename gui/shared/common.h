#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <QCoreApplication>
#include <QTcpServer>
#include <memory>
#include "error_codes.h"

namespace Tau5Common {
    // Binary type enum used for formatting help text and server info
    enum class BinaryType {
        Gui,      // tau5 GUI application
        Node      // tau5-node headless server
    };

    // Configuration constants
    namespace Config {
        // Port is always dynamically allocated for security
        constexpr const char* APP_NAME = "tau5";
        
        #ifdef TAU5_VERSION
            constexpr const char* APP_VERSION = TAU5_VERSION;
        #else
            constexpr const char* APP_VERSION = "0.0.0";
        #endif
        
        #ifdef TAU5_COMMIT
            constexpr const char* APP_COMMIT = TAU5_COMMIT;
        #else
            constexpr const char* APP_COMMIT = "unknown";
        #endif
        
        // Timer delays (in milliseconds)
        constexpr int BEAM_STARTUP_DELAY_MS = 2000;  // Delay before starting BEAM in GUI mode
        constexpr int NODE_STARTUP_DELAY_MS = 500;   // Delay before starting BEAM in node mode
    }

    // Get a free port for the server (deprecated - use allocatePort instead)
    [[deprecated("Use allocatePort() for race-free port allocation")]]
    quint16 getFreePort();
    
    // Allocate and hold a port to avoid race conditions
    // Returns a TcpServer that holds the port until you're ready to use it
    std::unique_ptr<QTcpServer> allocatePort(quint16& outPort, const QHostAddress& address = QHostAddress::Any);

    QString getServerBasePath(const std::string& commandLineOverride = "");
    QString resolveProductionServerPath(const QString& basePath, bool verbose = false);

    // Setup console output on Windows
    bool setupConsoleOutput();

    // Get Tau5 ASCII art logo
    QString getTau5Logo();

    // Setup console signal handling for graceful shutdown
    void setupSignalHandlers();
    
    // Setup Qt-dependent signal handling components (must be called after QCoreApplication creation)
    void setupSignalNotifier();
    
    // Check if a termination signal has been received
    bool isTerminationRequested();
    
    // Cleanup signal handling resources
    void cleanupSignalHandlers();
}

#endif // COMMON_H