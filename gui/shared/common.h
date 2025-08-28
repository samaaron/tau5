#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <QCoreApplication>
#include <QTcpServer>
#include <memory>
#include "error_codes.h"

namespace Tau5Common {
    // Configuration constants
    namespace Config {
        constexpr quint16 DEFAULT_PORT = 5555;
        constexpr const char* APP_NAME = "tau5";
        constexpr const char* APP_VERSION = "0.1.0";
    }

    // Get a free port for the server (deprecated - use allocatePort instead)
    [[deprecated("Use allocatePort() for race-free port allocation")]]
    quint16 getFreePort();
    
    // Allocate and hold a port to avoid race conditions
    // Returns a TcpServer that holds the port until you're ready to use it
    std::unique_ptr<QTcpServer> allocatePort(quint16& outPort, const QHostAddress& address = QHostAddress::Any);

    // Get the server base path relative to the executable
    QString getServerBasePath();

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