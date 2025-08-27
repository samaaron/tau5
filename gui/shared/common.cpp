#include "common.h"
#include <QTcpServer>
#include <QDir>
#include <iostream>
#include <csignal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace Tau5Common {

quint16 getFreePort() {
    QTcpServer server;
    if (server.listen(QHostAddress::Any, 0)) {
        quint16 freePort = server.serverPort();
        server.close();
        return freePort;
    }
    return 0;
}

std::unique_ptr<QTcpServer> allocatePort(quint16& outPort, const QHostAddress& address) {
    auto server = std::make_unique<QTcpServer>();
    
    // Try to listen on any available port
    if (server->listen(address, 0)) {
        outPort = server->serverPort();
        // Keep the server listening to hold the port
        // Caller can close() it when ready to use the port for something else
        // Or keep it as-is if they want to use this QTcpServer
        return server;
    }
    
    // Failed to allocate
    outPort = 0;
    return nullptr;
}

QString getServerBasePath() {
    // Always use environment variable if set
    QString serverPath = qEnvironmentVariable("TAU5_SERVER_PATH");
    if (!serverPath.isEmpty()) {
        if (!QDir(serverPath).exists()) {
            qWarning() << "TAU5_SERVER_PATH set but directory doesn't exist:" << serverPath;
        }
        return QDir(serverPath).absolutePath();
    }
    
    // Fallback if running without launch scripts (should rarely happen)
    qWarning() << "TAU5_SERVER_PATH not set - using fallback path detection";
    QString appDirPath = QCoreApplication::applicationDirPath();
    
    // Simple fallback: assume server is at ../server relative to repo structure
    QDir dir(appDirPath);
    while (!dir.isRoot() && dir.dirName() != "tau5") {
        if (!dir.cdUp()) break;
    }
    
    if (dir.dirName() == "tau5" && dir.cd("server")) {
        return dir.absolutePath();
    }
    
    // Last resort - return current directory
    qCritical() << "Could not find server directory. Set TAU5_SERVER_PATH environment variable.";
    return appDirPath;
}

bool setupConsoleOutput() {
#if defined(Q_OS_WIN)
    // Try to attach to parent process console first (when launched from cmd/powershell)
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* stream;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
        freopen_s(&stream, "CONIN$", "r", stdin);
        
        std::ios::sync_with_stdio();
        return true;
    } 
    // If no parent console, allocate a new one (for CLI mode)
    else if (AllocConsole()) {
        FILE* stream;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
        freopen_s(&stream, "CONIN$", "r", stdin);
        
        // Set console title
        SetConsoleTitleW(L"Tau5 Node");
        
        // Enable ANSI color codes on Windows 10+
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
        
        std::ios::sync_with_stdio();
        return true;
    }
    return false;
#else
    return true;  // Unix-like systems already have console output
#endif
}

QString getTau5Logo() {
    return R"(                            ╘
                    ─       ╛▒╛
                     ▐╫       ▄█├
              ─╟╛      █▄      ╪▓▀
    ╓┤┤┤┤┤┤┤┤┤  ╩▌      ██      ▀▓▌
     ▐▒   ╬▒     ╟▓╘    ─▓█      ▓▓├
     ▒╫   ▒╪      ▓█     ▓▓─     ▓▓▄
    ╒▒─  │▒       ▓█     ▓▓     ─▓▓─
    ╬▒   ▄▒ ╒    ╪▓═    ╬▓╬     ▌▓▄
    ╥╒   ╦╥     ╕█╒    ╙▓▐     ▄▓╫
               ▐╩     ▒▒      ▀▀
                    ╒╪      ▐▄

        ______           ______
       /_  __/___  __  _/ ____/
        / / / __ `/ / / /___ \
       / / / /_/ / /_/ /___/ /
      /_/  \__,_/\__,_/_____/

     Collaborative Live Coding
           for Everyone

)";
}

// Global flag for signal handling
static volatile std::sig_atomic_t g_signalReceived = 0;

static void signalHandler(int signal) {
    g_signalReceived = signal;
    // On Windows, we need to be more careful about exiting
#ifdef Q_OS_WIN
    // Generate a quit event for the Qt event loop
    if (qApp) {
        QCoreApplication::quit();
    }
#else
    QCoreApplication::quit();
#endif
}

void setupSignalHandlers() {
    // Install signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // Termination signal
    
#ifdef Q_OS_WIN
    // Windows-specific: Handle console events
    SetConsoleCtrlHandler([](DWORD dwCtrlType) -> BOOL {
        switch (dwCtrlType) {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
            case CTRL_LOGOFF_EVENT:
            case CTRL_SHUTDOWN_EVENT:
                signalHandler(SIGINT);
                return TRUE;
        }
        return FALSE;
    }, TRUE);
#else
    // Unix-like systems: Also handle SIGHUP (terminal hangup)
    std::signal(SIGHUP, signalHandler);
#endif
}

} // namespace Tau5Common