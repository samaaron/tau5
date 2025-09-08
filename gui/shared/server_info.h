#ifndef SERVER_INFO_H
#define SERVER_INFO_H

#include <QString>
#include <QtGlobal>
#include "common.h"

namespace Tau5Common {

/**
 * Server information structure used by both tau5 and tau5-node
 * Contains all runtime configuration and state needed for server info display
 */
struct ServerInfo {
    BinaryType binaryType = BinaryType::Node;
    bool isDevBuild = false;  // true for dev builds, false for release
    bool otpReady = false;
    quint16 serverPort = 0;
    quint16 publicPort = 0;
    QString mode;  // "development" or "production"
    qint64 nodePid = 0;
    qint64 beamPid = 0;
    QString logPath;
    QString sessionToken;
    bool hasLocalEndpoint = true;
    bool hasMcpEndpoint = false;
    quint16 mcpPort = 0;
    bool hasTidewave = false;
    bool hasRepl = false;
    QString friendToken;
};

/**
 * Generate complete server info as a formatted string
 * @param info Server information structure with runtime state
 * @param verbose If true, includes additional details for verbose mode
 * @return Formatted string ready to be printed with a single cout/logger call
 */
QString generateServerInfoString(const ServerInfo& info, bool verbose = false);

/**
 * Get public endpoint URLs with network interfaces
 * @param port Port number for the public endpoint
 * @param friendToken Optional friend authentication token
 * @return Formatted string with all available network interface URLs
 */
QString generatePublicEndpointsString(quint16 port, const QString& friendToken = QString());

/**
 * Convert development mode boolean to user-friendly string
 * @param isDevMode True for development mode, false for production
 * @return "development" or "production"
 */
QString getServerModeString(bool isDevMode);

} // namespace Tau5Common

#endif // SERVER_INFO_H