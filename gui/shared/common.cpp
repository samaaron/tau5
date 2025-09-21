#include "common.h"
#include "tau5logger.h"
#include <QTcpServer>
#include <QDir>
#include <QThread>
#include <QMetaObject>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <iostream>
#include <csignal>
#ifndef Q_OS_WIN
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <cstring>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace Tau5Common {

// Chrome CDP static variable definitions
namespace ChromeCDP {
    bool enabled = false;
    quint16 port = 0;
}

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

QString getServerBasePath(const std::string& commandLineOverride) {
    // Priority 1: Command-line override
    if (!commandLineOverride.empty()) {
        QString overridePath = QString::fromStdString(commandLineOverride);
        return QDir(overridePath).absolutePath();
    }
    
    // Priority 2: Environment variable
    QString serverPath = qEnvironmentVariable("TAU5_SERVER_PATH");
    if (!serverPath.isEmpty()) {
        return QDir(serverPath).absolutePath();
    }

    // Priority 3: Compiled-in default (if set at build time)
#ifdef TAU5_SERVER_PATH_DEFAULT
    QString defaultPath = QString(TAU5_SERVER_PATH_DEFAULT);
    // If it's a relative path, resolve it from the binary location
    if (QDir::isRelativePath(defaultPath)) {
        QDir appDir(QCoreApplication::applicationDirPath());
        return appDir.absoluteFilePath(defaultPath);
    }
    return defaultPath;
#else
    // No default configured - return empty string
    // The caller should handle this as a fatal error
    return QString();
#endif
}

bool setupConsoleOutput() {
#if defined(Q_OS_WIN)
    // Try to attach to parent process console first (when launched from cmd/powershell)
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        // Store original code page
        UINT originalCP = GetConsoleCP();
        UINT originalOutputCP = GetConsoleOutputCP();

        // Try to set console to UTF-8 mode
        // Note: This may not work properly with all console fonts
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        FILE* stream;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
        freopen_s(&stream, "CONIN$", "r", stdin);

        std::ios::sync_with_stdio();
        return true;
    }
    // If no parent console, allocate a new one (for CLI mode)
    else if (AllocConsole()) {
        // Set console to UTF-8 mode
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

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

       Code. Art. Together.

)";
}

static volatile std::sig_atomic_t g_signalReceived = 0;

#ifndef Q_OS_WIN
static int signalPipeFd[2] = {-1, -1};
static QSocketNotifier* signalNotifier = nullptr;

static void signalHandler(int signal) {
    g_signalReceived = signal;
    char a = 1;
    if (signalPipeFd[1] != -1) {
        ssize_t result = ::write(signalPipeFd[1], &a, sizeof(a));
        (void)result;
    }
}
#else
static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            g_signalReceived = SIGINT;
            if (qApp) {
                QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
            }
            return TRUE;
    }
    return FALSE;
}

static void signalHandler(int signal) {
    g_signalReceived = signal;
}
#endif

void setupSignalHandlers() {
#ifndef Q_OS_WIN
    if (::pipe(signalPipeFd) == -1) {
        std::cerr << "Failed to create signal pipe: " << strerror(errno) << std::endl;
        return;
    }

    auto set_nb_cloexec = [](int fd) {
        int flags = ::fcntl(fd, F_GETFL);
        if (flags != -1) {
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
        int fdflags = ::fcntl(fd, F_GETFD);
        if (fdflags != -1) {
            ::fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC);
        }
    };

    set_nb_cloexec(signalPipeFd[0]);
    set_nb_cloexec(signalPipeFd[1]);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGHUP, signalHandler);
#else
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
    std::signal(SIGTERM, signalHandler);
#endif
}

void setupSignalNotifier() {
#ifndef Q_OS_WIN
    if (!qApp) {
        std::cerr << "setupSignalNotifier called before QCoreApplication creation!" << std::endl;
        return;
    }

    if (signalPipeFd[0] == -1) {
        std::cerr << "setupSignalNotifier called before setupSignalHandlers!" << std::endl;
        return;
    }

    signalNotifier = new QSocketNotifier(signalPipeFd[0], QSocketNotifier::Read, qApp);
    QObject::connect(signalNotifier, &QSocketNotifier::activated, [](int) {
        char tmp;
        while (::read(signalPipeFd[0], &tmp, sizeof(tmp)) > 0) {}

        if (g_signalReceived != 0 && qApp) {
            qApp->quit();
        }
    });
#endif
}

bool isTerminationRequested() {
    return g_signalReceived != 0;
}

void cleanupSignalHandlers() {
#ifndef Q_OS_WIN
    if (signalNotifier) {
        delete signalNotifier;
        signalNotifier = nullptr;
    }

    if (signalPipeFd[0] != -1) {
        ::close(signalPipeFd[0]);
        signalPipeFd[0] = -1;
    }

    if (signalPipeFd[1] != -1) {
        ::close(signalPipeFd[1]);
        signalPipeFd[1] = -1;
    }
#endif
}

QString resolveProductionServerPath(const QString& basePath, bool verbose) {
#ifndef TAU5_RELEASE_BUILD
    if (!basePath.isEmpty()) {
        QString releasePath = QDir(basePath).absoluteFilePath("_build/prod/rel/tau5");
        if (QDir(releasePath).exists("bin/tau5")) {
            if (verbose) {
                Tau5Logger::instance().info(QString("Using production release at: %1").arg(releasePath));
            }
            return releasePath;
        }
        return basePath;
    } else {
        QString appDir = QCoreApplication::applicationDirPath();
        QDir searchDir(appDir);
        
        for (int i = 0; i < 5; i++) {
            QString candidatePath = searchDir.absoluteFilePath("server/_build/prod/rel/tau5");
            if (QDir(candidatePath).exists("bin/tau5")) {
                if (verbose) {
                    Tau5Logger::instance().info(QString("Auto-detected production release at: %1").arg(candidatePath));
                }
                return candidatePath;
            }
            if (!searchDir.cdUp()) break;
        }
        return QString();
    }
#else
    return basePath;
#endif
}

bool isPortAvailable(quint16 port) {
    QTcpServer testServer;
    bool available = testServer.listen(QHostAddress::LocalHost, port);
    testServer.close();
    return available;
}

} // namespace Tau5Common