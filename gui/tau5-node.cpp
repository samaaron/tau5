#include <iostream>
#include <memory>
#include <cstdlib>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>
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
              << "  --help, -h       Show this help message\n"
              << "\n"
              << "Tau5 Node - Headless server mode for Tau5\n"
              << "Starts the Phoenix server and keeps it running until interrupted.\n";
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    bool devMode = false;
    bool enableMcp = false;
    bool enableRepl = false;
    quint16 port = 0;
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "dev") == 0) {
            devMode = true;
        } else if (std::strcmp(argv[i], "--enable-mcp") == 0) {
            enableMcp = true;
        } else if (std::strcmp(argv[i], "--enable-repl") == 0) {
            enableRepl = true;
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
    
    // Setup console output (especially important on Windows)
    setupConsoleOutput();
    
    // Setup signal handlers for graceful shutdown
    setupSignalHandlers();
    
    // Create Qt Core application (no GUI)
    QCoreApplication app(argc, argv);
    app.setApplicationName(Config::APP_NAME);
    
    // Configure logging
    Tau5LoggerConfig logConfig;
    logConfig.appName = "node";  // Different from GUI
    logConfig.logFiles = {
        {"node.log", "node", false},
        {"beam.log", "beam", false}
    };
    logConfig.emitQtSignals = false;  // No GUI to connect to
    logConfig.consoleEnabled = true;
    logConfig.consoleColors = true;  // Always use colors in CLI mode
    logConfig.reuseRecentSession = false;
    
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    logConfig.baseLogDir = QDir(dataPath).absoluteFilePath("Tau5/logs");
    
    // Initialize logger
    Tau5Logger::initialize(logConfig);
    
    // Install Qt message handler
    originalMessageHandler = qInstallMessageHandler(tau5NodeMessageHandler);
    
    // Print startup banner
    Tau5Logger::instance().info(getTau5Logo());
    Tau5Logger::instance().info("Starting Tau5 Node (Headless Mode)...");
    
    // Determine port
    if (port == 0) {
        if (devMode) {
            port = Config::DEFAULT_PORT;
            Tau5Logger::instance().info("Development mode enabled");
        } else {
            quint16 allocatedPort = 0;
            auto portHolder = allocatePort(allocatedPort);
            if (!portHolder || allocatedPort == 0) {
                Tau5Logger::instance().error("Failed to allocate port");
                return static_cast<int>(ExitCode::PORT_ALLOCATION_FAILED);
            }
            port = allocatedPort;
            // Close the server to release the port for the BEAM process
            portHolder->close();
            Tau5Logger::instance().info("Production mode enabled");
        }
    }
    
    Tau5Logger::instance().info(QString("Using port: %1").arg(port));
    
    if (enableMcp) {
        Tau5Logger::instance().info(QString("MCP servers enabled (%1)").arg(
            devMode ? "tidewave for development" : "hermes for production"));
    }
    
    if (enableRepl) {
        Tau5Logger::instance().info("Elixir REPL console enabled");
    }
    
    // Get server base path
    QString basePath = getServerBasePath();
    Tau5Logger::instance().info(QString("Server base path: %1").arg(basePath));
    
    if (!QDir(basePath).exists()) {
        Tau5Logger::instance().error("Server directory not found at: " + basePath);
        return static_cast<int>(ExitCode::SERVER_DIR_NOT_FOUND);
    }
    
    // Create BEAM instance
    std::shared_ptr<Beam> beam;
    
    // Small delay to ensure everything is initialized
    QTimer::singleShot(500, [&app, &beam, basePath, port, devMode, enableMcp, enableRepl]() {
        Tau5Logger::instance().info("Starting BEAM server...");
        
        beam = std::make_shared<Beam>(&app, basePath, Config::APP_NAME,
                                     Config::APP_VERSION, port, devMode, 
                                     enableMcp, enableRepl);
        
        // Connect to OTP ready signal
        QObject::connect(beam.get(), &Beam::otpReady, []() {
            Tau5Logger::instance().info("OTP supervision tree ready");
            Tau5Logger::instance().info("Server is running. Press Ctrl+C to stop.");
        });
        
        // Connect standard output/error for visibility
        QObject::connect(beam.get(), &Beam::standardOutput, [](const QString& output) {
            // Log BEAM output to the beam category
            Tau5Logger::instance().log(LogLevel::Info, "beam", output);
        });
        
        QObject::connect(beam.get(), &Beam::standardError, [](const QString& error) {
            // Log BEAM errors to the beam category
            Tau5Logger::instance().log(LogLevel::Error, "beam", error);
        });
    });
    
    // Ensure BEAM is terminated before the app fully exits
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&beam]() {
        Tau5Logger::instance().info("Shutting down Tau5 Node...");
        if (beam) {
            beam.reset(); // Explicitly destroy beam while Qt is still running
        }
        Tau5Logger::instance().info("Tau5 Node stopped");
    });
    
    // Run the event loop
    int result = app.exec();
    
    // Clean shutdown message
    if (result == 0) {
        std::cout << "\nTau5 Node shutdown complete\n";
    }
    
    return result;
}