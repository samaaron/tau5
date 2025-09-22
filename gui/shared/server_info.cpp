#include "server_info.h"
#include <QNetworkInterface>
#include <QHostAddress>
#include <QStringList>
#include <QTextStream>

namespace Tau5Common {

QString getServerModeString(bool isDevMode) {
    return isDevMode ? "development" : "production";
}

QString generatePublicEndpointsString(quint16 port, const QString& friendToken) {
    QStringList lines;
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    bool firstPublicIP = true;
    bool firstFriendIP = true;
    bool hasFriendMode = !friendToken.isEmpty();
    
    for (const QHostAddress &address : addresses) {
        if (!address.isLoopback() && 
            address.protocol() == QAbstractSocket::IPv4Protocol) {
            
            if (firstPublicIP) {
                lines << QString("  Public:    http://%1:%2").arg(address.toString()).arg(port);
                firstPublicIP = false;
            } else {
                lines << QString("             http://%1:%2").arg(address.toString()).arg(port);
            }
            
            if (hasFriendMode) {
                if (firstFriendIP) {
                    lines << QString("  Friend:    http://%1:%2/?friend_token=%3")
                        .arg(address.toString()).arg(port).arg(friendToken);
                    firstFriendIP = false;
                } else {
                    lines << QString("             http://%1:%2/?friend_token=%3")
                        .arg(address.toString()).arg(port).arg(friendToken);
                }
            }
        }
    }
    
    // If no non-loopback addresses found, show 127.0.0.1 as fallback
    if (firstPublicIP) {
        lines << QString("  Public:    http://127.0.0.1:%1").arg(port);
        if (hasFriendMode) {
            lines << QString("  Friend:    http://127.0.0.1:%1/?friend_token=%2")
                .arg(port).arg(friendToken);
        }
    }
    
    return lines.join("\n");
}

QString generateServerInfoString(const ServerInfo& info, bool verbose) {
    QString result;
    QTextStream stream(&result);
    
    stream << "\n";
    stream << "========================================================\n";
    
    QString title;
    if (info.binaryType == BinaryType::Gui) {
        title = info.isDevBuild ? "Tau5 Development" : "Tau5";
    } else {
        title = info.isDevBuild ? "Tau5 Node (Development)" : "Tau5 Server";
    }
    stream << title << " Started\n";
    
    stream << "--------------------------------------------------------\n";
    
    stream << "  Mode:      " << info.mode << "\n";
    
    if (!info.hasLocalEndpoint) {
        stream << "  Local:     Disabled (--no-local-endpoint)\n";
    } else if (info.serverPort > 0) {
        stream << "  Local:     http://localhost:" << info.serverPort;
        if (!info.sessionToken.isEmpty()) {
            stream << "/?token=" << info.sessionToken;
        }
        stream << "\n";
        
        // Dashboard for tau5-node (not shown in verbose mode as it's in logs)
        if (info.binaryType == BinaryType::Node && !info.sessionToken.isEmpty() && !verbose) {
            stream << "  Dashboard: http://localhost:" << info.serverPort 
                   << "/dev/dashboard?token=" << info.sessionToken << "\n";
        }
    } else {
        stream << "  Local:     (port allocation in progress)\n";
    }
    
    if (info.publicPort > 0) {
        stream << generatePublicEndpointsString(info.publicPort, info.friendToken) << "\n";
    }
    
    stream << "  Node PID:  " << info.nodePid << "\n";
    if (info.beamPid > 0) {
        stream << "  BEAM PID:  " << info.beamPid << "\n";
    }
    
    stream << "  Logs:      " << info.logPath << "\n";

    if (info.channel > 0) {
        stream << "  Channel:   " << info.channel << "\n";
    }

    if (info.hasMcpEndpoint) {
        stream << "  MCP:       Port " << info.mcpPort;
        if (info.hasTidewave) {
            stream << " (with Tidewave)";
        }
        stream << "\n";
    }

    if (info.hasChromeDevtools && info.binaryType == BinaryType::Gui) {
        stream << "  Chrome CDP: Port " << info.chromePort << "\n";
    }

    if (info.hasRepl && !info.sessionToken.isEmpty() && info.hasLocalEndpoint) {
        stream << "  Console:   http://localhost:" << info.serverPort
               << "/dev/console?token=" << info.sessionToken << "\n";
    }
    
    stream << "========================================================\n";
    
    if (info.binaryType == BinaryType::Gui && !verbose) {
        // GUI in quiet mode doesn't show "Press Ctrl+C" since it has window controls
        stream << "\n";
    } else {
        stream << "Press Ctrl+C to stop\n";
    }
    
    stream.flush();
    return result;
}

} // namespace Tau5Common