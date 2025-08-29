#include <iostream>
#include <memory>
#include <cstdlib>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>
#ifndef Q_OS_WIN
#include <unistd.h>
#else
#include <windows.h>
#include <process.h>
#endif
#include "shared/beam.h"
#include "shared/tau5logger.h"
#include "shared/common.h"

using namespace Tau5Common;

static QtMessageHandler originalMessageHandler = nullptr;

void tau5NodeMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    if (originalMessageHandler) {
        originalMessageHandler(type, context, msg);
    }

    switch (type) {
    case QtDebugMsg:
        Tau5Logger::instance().debug(QString("[Qt] %1").arg(msg));
        break;
    case QtInfoMsg:
        Tau5Logger::instance().info(QString("[Qt] %1").arg(msg));
        break;
    case QtWarningMsg:
        Tau5Logger::instance().warning(QString("[Qt] %1").arg(msg));
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        Tau5Logger::instance().error(QString("[Qt] %1").arg(msg));
        break;
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  dev              Run in development mode\n"
              << "  --enable-mcp     Enable MCP servers (tidewave in dev, hermes in prod)\n"
              << "  --enable-repl    Enable Elixir REPL console\n"
              << "  --port <number>  Specify server port (default: 5555 in dev, random in prod)\n"
              << "  --verbose        Enable verbose logging (show all BEAM output)\n"
              << "\n"
              << "Local I/O Services (requires compiled NIFs):\n"
              << "  --disable-midi   Disable MIDI support\n"
              << "  --disable-link   Disable Ableton Link support\n"
              << "  --disable-discovery  Disable network discovery\n"
              << "  --disable-all    Disable all local I/O services\n"
              << "\n"
              << "  --help, -h       Show this help message\n"
              << "\n"
              << "Tau5 Node - Headless server mode for Tau5\n"
              << "Starts the Phoenix server and keeps it running until interrupted.\n"
              << "\n"
              << "Note: MIDI, Link, and Discovery services require the corresponding NIFs\n"
              << "to be compiled during the build. If a NIF is not available, the service\n"
              << "will show as 'Module Missing' in the summary.\n";
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    bool devMode = false;
    bool enableMcp = false;
    bool enableRepl = false;
    bool verboseMode = false;
    bool disableMidi = false;
    bool disableLink = false;
    bool disableDiscovery = false;
    quint16 port = 0;
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "dev") == 0) {
            devMode = true;
        } else if (std::strcmp(argv[i], "--enable-mcp") == 0) {
            enableMcp = true;
        } else if (std::strcmp(argv[i], "--enable-repl") == 0) {
            enableRepl = true;
        } else if (std::strcmp(argv[i], "--verbose") == 0) {
            verboseMode = true;
        } else if (std::strcmp(argv[i], "--disable-midi") == 0) {
            disableMidi = true;
        } else if (std::strcmp(argv[i], "--disable-link") == 0) {
            disableLink = true;
        } else if (std::strcmp(argv[i], "--disable-discovery") == 0) {
            disableDiscovery = true;
        } else if (std::strcmp(argv[i], "--disable-all") == 0) {
            disableMidi = true;
            disableLink = true;
            disableDiscovery = true;
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = static_cast<quint16>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            printUsage(argv[0]);
            return static_cast<int>(ExitCode::INVALID_ARGUMENTS);
        }
    }
    
    setupConsoleOutput();
    setupSignalHandlers();
    
    QCoreApplication app(argc, argv);
    app.setApplicationName(Config::APP_NAME);
    Tau5Common::setupSignalNotifier();
    
    Tau5LoggerConfig logConfig;
    logConfig.appName = "node";
    logConfig.logFiles = {
        {"node.log", "node", false},
        {"beam.log", "beam", false}
    };
    logConfig.emitQtSignals = false;
    logConfig.consoleEnabled = verboseMode;
    logConfig.consoleColors = true;
    logConfig.reuseRecentSession = false;
    
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    logConfig.baseLogDir = QDir(dataPath).absoluteFilePath("Tau5/logs");
    
    Tau5Logger::initialize(logConfig);
    
    originalMessageHandler = qInstallMessageHandler(tau5NodeMessageHandler);
    
    if (!verboseMode) {
        std::cout << getTau5Logo().toLocal8Bit().constData();
        std::cout << "Starting Tau5 Node (Headless Mode)...\n" << std::flush;
    } else {
        Tau5Logger::instance().info(getTau5Logo());
        Tau5Logger::instance().info("Starting Tau5 Node (Headless Mode)...");
    }
    
    // Determine port
    if (port == 0) {
        if (devMode) {
            port = Config::DEFAULT_PORT;
            if (verboseMode) {
                Tau5Logger::instance().info("Development mode enabled");
            }
        } else {
            quint16 allocatedPort = 0;
            auto portHolder = allocatePort(allocatedPort);
            if (!portHolder || allocatedPort == 0) {
                if (verboseMode) {
                    Tau5Logger::instance().error("Failed to allocate port");
                } else {
                    std::cerr << "Error: Failed to allocate port\n";
                }
                return static_cast<int>(ExitCode::PORT_ALLOCATION_FAILED);
            }
            port = allocatedPort;
            // Close the server to release the port for the BEAM process
            portHolder->close();
            if (verboseMode) {
                Tau5Logger::instance().info("Production mode enabled");
            }
        }
    }
    
    if (verboseMode) {
        Tau5Logger::instance().info(QString("Using port: %1").arg(port));
        
        if (enableMcp) {
            Tau5Logger::instance().info(QString("MCP servers enabled (%1)").arg(
                devMode ? "tidewave for development" : "hermes for production"));
        }
        
        if (enableRepl) {
            Tau5Logger::instance().info("Elixir REPL console enabled");
        }
    }
    
    // Get server base path
    QString basePath = getServerBasePath();
    if (verboseMode) {
        Tau5Logger::instance().info(QString("Server base path: %1").arg(basePath));
    }
    
    if (!QDir(basePath).exists()) {
        if (verboseMode) {
            Tau5Logger::instance().error("Server directory not found at: " + basePath);
        } else {
            std::cerr << "Error: Server directory not found at: " << basePath.toStdString() << "\n";
        }
        return static_cast<int>(ExitCode::SERVER_DIR_NOT_FOUND);
    }
    
    // Create BEAM instance
    std::shared_ptr<Beam> beam;
    
    // Track service availability for summary
    struct {
        bool otpReady = false;
        quint16 serverPort = 0;
        QString mode;
        qint64 nodePid = 0;
        qint64 beamPid = 0;
        QString logPath;
    } serverInfo;
    
    serverInfo.serverPort = port;
    serverInfo.mode = devMode ? "development" : "production";
    
    // Get Node PID
    #ifndef Q_OS_WIN
    serverInfo.nodePid = getpid();
    #else
    serverInfo.nodePid = _getpid();
    #endif
    
    // Get log path from logger
    serverInfo.logPath = Tau5Logger::instance().currentSessionPath();
    
    // Small delay to ensure everything is initialized
    QTimer::singleShot(500, [&app, &beam, basePath, port, devMode, enableMcp, enableRepl, verboseMode, disableMidi, disableLink, disableDiscovery, &serverInfo]() {
        if (verboseMode) {
            Tau5Logger::instance().info("Starting BEAM server...");
        }
        
        // Set environment variables for service control
        if (disableMidi) {
            qputenv("TAU5_MIDI_ENABLED", "false");
        }
        if (disableLink) {
            qputenv("TAU5_LINK_ENABLED", "false");
        }
        if (disableDiscovery) {
            qputenv("TAU5_DISCOVERY_ENABLED", "false");
        }
        
        beam = std::make_shared<Beam>(&app, basePath, Config::APP_NAME,
                                     Config::APP_VERSION, port, devMode, 
                                     enableMcp, enableRepl);
        
        // Connect to OTP ready signal
        QObject::connect(beam.get(), &Beam::otpReady, [verboseMode, enableMcp, &serverInfo, &beam]() {
            serverInfo.otpReady = true;
            
            // Get BEAM PID when it's ready
            if (beam) {
                serverInfo.beamPid = beam->getBeamPid();
            }
            
            if (verboseMode) {
                Tau5Logger::instance().info("OTP supervision tree ready");
                Tau5Logger::instance().info("Server is running. Press Ctrl+C to stop.");
            } else {
                // Print concise summary in quiet mode
                std::cout << "\n========================================================\n";
                std::cout << "Tau5 Server Started\n";
                std::cout << "--------------------------------------------------------\n";
                std::cout << "  Mode:      " << serverInfo.mode.toStdString() << "\n";
                std::cout << "  Port:      " << serverInfo.serverPort << "\n";
                std::cout << "  URL:       http://localhost:" << serverInfo.serverPort << "\n";
                std::cout << "  Node PID:  " << serverInfo.nodePid << "\n";
                if (serverInfo.beamPid > 0) {
                    std::cout << "  BEAM PID:  " << serverInfo.beamPid << "\n";
                }
                std::cout << "  Logs:      " << serverInfo.logPath.toStdString() << "\n";
                
                if (enableMcp) {
                    std::cout << "  MCP:       " << (serverInfo.mode == "development" ? "tidewave" : "hermes") << "\n";
                }
                
                std::cout << "========================================================\n";
                std::cout << "Press Ctrl+C to stop\n\n" << std::flush;
            }
        });
        
        // Connect standard output/error for visibility
        QObject::connect(beam.get(), &Beam::standardOutput, [verboseMode](const QString& output) {
            // Log BEAM output to the beam category only in verbose mode
            if (verboseMode) {
                Tau5Logger::instance().log(LogLevel::Info, "beam", output);
            }
        });
        
        QObject::connect(beam.get(), &Beam::standardError, [verboseMode](const QString& error) {
            // Log BEAM errors to the beam category only in verbose mode
            if (verboseMode) {
                Tau5Logger::instance().log(LogLevel::Error, "beam", error);
            } else {
                // In quiet mode, still show critical errors to stderr
                if (error.contains("ERROR") || error.contains("CRITICAL") || error.contains("FATAL")) {
                    std::cerr << "Error: " << error.toStdString();
                }
            }
        });
    });
    
    // Ensure BEAM is terminated before the app fully exits
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&beam, verboseMode]() {
        if (verboseMode) {
            Tau5Logger::instance().info("Shutting down Tau5 Node...");
        } else {
            std::cout << "\nShutting down Tau5 Node...\n";
        }
        if (beam) {
            beam.reset();
        }
        Tau5Common::cleanupSignalHandlers();
        if (verboseMode) {
            Tau5Logger::instance().info("Tau5 Node stopped");
        }
    });
    
    int result = app.exec();
    
    if (result == 0) {
        std::cout << "\nTau5 Node shutdown complete\n";
    }
    
    return result;
}