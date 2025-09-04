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
#include <io.h>
#include <fcntl.h>
#endif
#include "shared/beam.h"
#include "shared/tau5logger.h"
#include "shared/common.h"
#include "shared/cli_args.h"
#include "shared/health_check.h"

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
              << "\n"
              << "Quick Setup:\n"
              << "  --devtools               All-in-one dev setup (dev mode + MCP + Tidewave + REPL)\n"
              << "\n"
              << "Environment Selection:\n"
              << "  --env-dev                Development environment (MIX_ENV=dev)\n"
              << "  --env-prod               Production environment (MIX_ENV=prod) [default]\n"
              << "  --env-test               Test environment (MIX_ENV=test)\n"
              << "\n"
              << "Deployment Mode Override:\n"
              << "  --mode-node              Local headless server [default]\n"
              << "                           - Local and MCP endpoints available\n"
              << "                           - Full NIFs and local I/O support\n"
              << "  --mode-central           Public coordinator (tau5.live)\n"
              << "                           - Public web endpoints only\n"
              << "                           - No local endpoints or MCP servers\n"
              << "                           - No NIFs or local I/O capabilities\n"
              << "\n"
              << "Port Configuration:\n"
              << "  --port-local <n>         Local web UI port (default: random)\n"
              << "  --port-public <n>        Public endpoint port (default: disabled)\n"
              << "  --port-mcp <n>           MCP services port (default: 5555 when enabled)\n"
              << "\n"
              << "Optional Features:\n"
              << "  --mcp                    Enable MCP endpoint\n"
              << "  --tidewave               Add Tidewave to MCP endpoint (implies --mcp)\n"
              << "  --repl                   Enable Elixir REPL (dev mode only)\n"
              << "  --verbose                Enable verbose logging\n"
              << "\n"
              << "Disable Features:\n"
              << "  --no-midi                Disable MIDI support\n"
              << "  --no-link                Disable Ableton Link support\n"
              << "  --no-discovery           Disable network discovery\n"
              << "  --no-nifs                Disable all NIFs (MIDI, Link, and Discovery)\n"
              << "\n"
              << "Other:\n"
              << "  --server-path <path>     Override server directory path\n"
              << "  --help, -h               Show this help message\n"
              << "  --version                Show version information\n"
              << "\n"
              << "Tau5 - Code. Art. Together.\n"
              << "\n"
              << "Tau5 Node - Headless server mode for Tau5\n"
              << "\n";

}

int main(int argc, char *argv[]) {
#ifdef Q_OS_WIN
    // Set console to UTF-8 mode immediately on Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Set stdout to binary mode to prevent any text translation
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    // Parse command line arguments
    Tau5CLI::CommonArgs args;

    for (int i = 1; i < argc; ++i) {
        const char* nextArg = (i + 1 < argc) ? argv[i + 1] : nullptr;

        if (Tau5CLI::parseSharedArg(argv[i], nextArg, i, args)) {
            if (args.hasError) {
                std::cerr << "Error: " << args.errorMessage << "\n";
                printUsage(argv[0]);
                return static_cast<int>(ExitCode::INVALID_ARGUMENTS);
            }
            if (args.showHelp) {
                printUsage(argv[0]);
                return 0;
            }
            if (args.showVersion) {
                std::cout << "tau5-node version " << Config::APP_VERSION << "\n";
                return 0;
            }
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            printUsage(argv[0]);
            return static_cast<int>(ExitCode::INVALID_ARGUMENTS);
        }
    }

    setupConsoleOutput();
    setupSignalHandlers();

    // Validate central mode restrictions
    if (args.mode == Tau5CLI::CommonArgs::Mode::Central) {
        if (args.portLocal > 0) {
            std::cerr << "Error: --target-central does not support --port-local\n";
            std::cerr << "Central mode only uses public endpoints\n";
            return 1;
        }
        if (args.mcp) {
            std::cerr << "Error: --target-central does not support MCP services\n";
            std::cerr << "Central mode does not provide MCP servers\n";
            return 1;
        }
        if (args.tidewave) {
            std::cerr << "Error: --target-central does not support --with-tidewave\n";
            std::cerr << "Central mode does not provide development tools\n";
            return 1;
        }
        if (args.repl) {
            std::cerr << "Error: --target-central does not support --with-repl\n";
            std::cerr << "Central mode does not provide development tools\n";
            return 1;
        }
    }

    // Validate arguments for conflicts and dependencies
    if (!Tau5CLI::validateArguments(args)) {
        std::cerr << "Error: " << args.errorMessage << "\n";
        return 1;
    }

    // Handle --check flag for health check
    if (args.check) {
        // Initialize app for health check
        QCoreApplication app(argc, argv);
        app.setApplicationName(Config::APP_NAME);

        // Initialize logger for health check
        Tau5LoggerConfig logConfig;
        logConfig.appName = "node";
        logConfig.logFiles = {
            {"node.log", "node", false},
            {"beam.log", "beam", false}
        };
        logConfig.emitQtSignals = false;
        logConfig.consoleEnabled = true;
        logConfig.consoleColors = true;
        logConfig.reuseRecentSession = false;
        logConfig.baseLogDir = Tau5Logger::getBaseLogDir();
        Tau5Logger::initialize(logConfig);

        // Get server path
        QString basePath = getServerBasePath(args.serverPath);

        // Run health check
        Tau5HealthCheck::HealthCheckConfig checkConfig;
        checkConfig.serverPath = basePath;
        checkConfig.binaryName = "tau5-node";
        checkConfig.isGui = false;
        checkConfig.verbose = args.verbose;
        checkConfig.strictMode = false;
        checkConfig.runTests = args.verbose;  // Run tests in verbose mode
        checkConfig.testPort = 0;

        return Tau5HealthCheck::runHealthCheck(checkConfig);
    }

    // Apply service disable settings before starting
    // Default target for tau5-node is "node" unless overridden
    const char* targetName = "node";
    if (args.mode == Tau5CLI::CommonArgs::Mode::Central) {
        targetName = "central";
    }
    Tau5CLI::applyEnvironmentVariables(args, targetName);

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
    logConfig.consoleEnabled = args.verbose || (qgetenv("TAU5_VERBOSE") == "true");
    logConfig.consoleColors = true;
    logConfig.reuseRecentSession = false;

    logConfig.baseLogDir = Tau5Logger::getBaseLogDir();

    Tau5Logger::initialize(logConfig);

    originalMessageHandler = qInstallMessageHandler(tau5NodeMessageHandler);

    bool isDevMode = (args.env == Tau5CLI::CommonArgs::Env::Dev);

    if (!args.verbose) {
        std::cout << getTau5Logo().toUtf8().constData();
        std::cout << "Starting Tau5 Node (Headless Mode)...\n" << std::flush;
    } else {
        Tau5Logger::instance().info(getTau5Logo());
        Tau5Logger::instance().info("Starting Tau5 Node (Headless Mode)...");
    }

    // Determine port (central mode doesn't use local ports)
    quint16 port = 0;
    bool isCentralMode = (args.mode == Tau5CLI::CommonArgs::Mode::Central);

    if (!isCentralMode) {
        port = args.portLocal;
        if (port == 0) {
            if (isDevMode) {
                port = 0;  // Will allocate random port
                if (args.verbose) {
                    Tau5Logger::instance().info("Development mode enabled");
                }
            } else {
                quint16 allocatedPort = 0;
                auto portHolder = allocatePort(allocatedPort);
                if (!portHolder || allocatedPort == 0) {
                    if (args.verbose) {
                        Tau5Logger::instance().error("Failed to allocate port");
                    } else {
                        std::cerr << "Error: Failed to allocate port\n";
                    }
                    return static_cast<int>(ExitCode::PORT_ALLOCATION_FAILED);
                }
                port = allocatedPort;
                // Close the server to release the port for the BEAM process
                portHolder->close();
                if (args.verbose) {
                    Tau5Logger::instance().info("Production mode enabled");
                }
            }
        }
    } else {
        if (args.verbose) {
            Tau5Logger::instance().info("Central coordinator mode enabled");
        }
    }

    if (args.verbose) {
        Tau5Logger::instance().info(QString("Using port: %1").arg(port));

        QString mcpPort = qgetenv("TAU5_MCP_PORT");
        if (!mcpPort.isEmpty() && mcpPort != "0") {
            Tau5Logger::instance().info(QString("MCP endpoint enabled on port %1").arg(mcpPort));
            if (qgetenv("TAU5_TIDEWAVE_ENABLED") == "true") {
                Tau5Logger::instance().info("Tidewave MCP server enabled");
            }
        }

        if (qgetenv("TAU5_ELIXIR_REPL_ENABLED") == "true") {
            Tau5Logger::instance().info("Elixir REPL console enabled");
        }
    }

    // Get server base path
    QString basePath = getServerBasePath(args.serverPath);

    // Check if server path is configured
    if (basePath.isEmpty()) {
        if (args.verbose) {
            Tau5Logger::instance().error("FATAL: No server path configured");
            Tau5Logger::instance().error("Please specify server location using one of:");
            Tau5Logger::instance().error("  --server-path /path/to/server");
            Tau5Logger::instance().error("  TAU5_SERVER_PATH environment variable");
        } else {
            std::cerr << "FATAL: No server path configured\n";
            std::cerr << "Please specify server location using one of:\n";
            std::cerr << "  --server-path /path/to/server\n";
            std::cerr << "  TAU5_SERVER_PATH environment variable\n";
        }
        return static_cast<int>(ExitCode::SERVER_DIR_NOT_FOUND);
    }

    if (args.verbose) {
        Tau5Logger::instance().info(QString("Server path: %1").arg(basePath));
    }

    // Verify the path exists
    if (!QDir(basePath).exists()) {
        if (args.verbose) {
            Tau5Logger::instance().error(QString("FATAL: Server directory not found: %1").arg(basePath));
        } else {
            std::cerr << "FATAL: Server directory not found: " << basePath.toStdString() << "\n";
        }
        return static_cast<int>(ExitCode::SERVER_DIR_NOT_FOUND);
    }

    // Validate that the requested mode matches the build type
#ifdef TAU5_RELEASE_BUILD
    // Release builds must run in production mode
    if (isDevMode) {
        if (args.verbose) {
            Tau5Logger::instance().error("Cannot use --env-dev with a release build.");
            Tau5Logger::instance().error("Release builds only support production mode.");
        } else {
            std::cerr << "Error: Cannot use --env-dev with a release build.\n";
            std::cerr << "Release builds only support production mode.\n";
        }
        return static_cast<int>(ExitCode::INVALID_ARGUMENTS);
    }
#else
    // Development build - validate server structure matches requested mode
    QDir serverDir(basePath);
    
    bool hasSourceStructure = serverDir.exists("mix.exs");
    bool hasReleaseStructure = serverDir.exists("bin/tau5");
    
    if (isDevMode && !hasSourceStructure) {
        if (args.verbose) {
            Tau5Logger::instance().error("--env-dev requires source structure (mix.exs) but not found");
            Tau5Logger::instance().error(QString("Server path: %1").arg(basePath));
            Tau5Logger::instance().error("This appears to be a release structure. Use --env-prod instead.");
        } else {
            std::cerr << "Error: --env-dev requires source structure (mix.exs) but not found at:\n";
            std::cerr << "  " << basePath.toStdString() << "\n";
            std::cerr << "This appears to be a release structure. Use --env-prod instead.\n";
        }
        return static_cast<int>(ExitCode::INVALID_ARGUMENTS);
    }
    
    if (!isDevMode && !hasReleaseStructure) {
        // Only error if we also don't have source structure
        if (!hasSourceStructure) {
            if (args.verbose) {
                Tau5Logger::instance().error("--env-prod requires release structure but server directory is invalid");
                Tau5Logger::instance().error(QString("Server path: %1").arg(basePath));
            } else {
                std::cerr << "Error: --env-prod requires release structure but server directory is invalid:\n";
                std::cerr << "  " << basePath.toStdString() << "\n";
            }
            return static_cast<int>(ExitCode::SERVER_DIR_NOT_FOUND);
        }
        // If we have source structure but no release, give helpful message
        if (args.verbose) {
            Tau5Logger::instance().error("--env-prod requires release structure (bin/tau5) but not found");
            Tau5Logger::instance().error(QString("To create a production release:"));
            Tau5Logger::instance().error(QString("  cd %1").arg(basePath));
            Tau5Logger::instance().error("  MIX_ENV=prod mix deps.get --only prod");
            Tau5Logger::instance().error("  MIX_ENV=prod mix compile");
            Tau5Logger::instance().error("  MIX_ENV=prod mix release");
            Tau5Logger::instance().error("Or use --env-dev for development mode.");
        } else {
            std::cerr << "Error: --env-prod requires release structure (bin/tau5) but not found.\n";
            std::cerr << "To create a production release:\n";
            std::cerr << "  cd " << basePath.toStdString() << "\n";
            std::cerr << "  MIX_ENV=prod mix deps.get --only prod\n";
            std::cerr << "  MIX_ENV=prod mix compile\n";
            std::cerr << "  MIX_ENV=prod mix release\n";
            std::cerr << "Or use --env-dev for development mode.\n";
        }
        return static_cast<int>(ExitCode::INVALID_ARGUMENTS);
    }
#endif

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
        QString sessionToken;
    } serverInfo;

    serverInfo.serverPort = port;
    serverInfo.mode = isDevMode ? "development" : "production";

    // Get Node PID
    #ifndef Q_OS_WIN
    serverInfo.nodePid = getpid();
    #else
    serverInfo.nodePid = _getpid();
    #endif

    // Get log path from logger
    serverInfo.logPath = Tau5Logger::instance().currentSessionPath();

    // Small delay to ensure everything is initialized
    QTimer::singleShot(Tau5Common::Config::NODE_STARTUP_DELAY_MS, [&app, &beam, basePath, port, &args, &serverInfo, isDevMode, isCentralMode]() {
        if (args.verbose) {
            Tau5Logger::instance().info("Starting BEAM server...");
        }

        // Create Beam instance with deploymentMode based on --central flag
        bool enableMcp = (qgetenv("TAU5_MCP_PORT") != "0" && !qgetenv("TAU5_MCP_PORT").isEmpty());
        bool enableRepl = (qgetenv("TAU5_ELIXIR_REPL_ENABLED") == "true");
        Beam::DeploymentMode mode = isCentralMode ? Beam::DeploymentMode::Central : Beam::DeploymentMode::Node;
        beam = std::make_shared<Beam>(&app, basePath, Config::APP_NAME,
                                     Config::APP_VERSION, port, isDevMode,
                                     enableMcp, enableRepl, mode);

        // Get session token from beam
        serverInfo.sessionToken = beam->getSessionToken();

        // Track whether we've shown the server info yet
        bool serverInfoShown = false;

        // Connect to actualPortAllocated signal to update the port when we get it
        QObject::connect(beam.get(), &Beam::actualPortAllocated, [&serverInfo, &args, port](quint16 actualPort) {
            if (actualPort > 0) {
                serverInfo.serverPort = actualPort;
                if (args.verbose) {
                    Tau5Logger::instance().info(QString("Server allocated port: %1").arg(actualPort));
                }
            }
        });

        // Connect to OTP ready signal
        QObject::connect(beam.get(), &Beam::otpReady, [&args, &serverInfo, &beam, &serverInfoShown, port]() {
            serverInfo.otpReady = true;

            // Get BEAM PID when it's ready
            if (beam) {
                serverInfo.beamPid = beam->getBeamPid();
                // Also update port in case it was allocated
                quint16 actualPort = beam->getPort();
                if (actualPort > 0) {
                    serverInfo.serverPort = actualPort;
                }
            }

            // For random port (0), wait a bit for the actual port to be reported
            if (port == 0 && serverInfo.serverPort == 0 && !serverInfoShown) {
                // Set a timer to show server info after a short delay to allow port allocation
                QTimer::singleShot(300, [&serverInfo, &args, &serverInfoShown]() {
                    if (!serverInfoShown) {
                        serverInfoShown = true;
                        if (!args.verbose) {
                            // Print server summary
                            std::cout << "\n========================================================\n";
                            std::cout << "Tau5 Server Started\n";
                            std::cout << "--------------------------------------------------------\n";
                            std::cout << "  Mode:      " << serverInfo.mode.toStdString() << "\n";
                            if (serverInfo.serverPort > 0) {
                                std::cout << "  Port:      " << serverInfo.serverPort << "\n";
                                if (serverInfo.mode == "development" && !serverInfo.sessionToken.isEmpty()) {
                                    std::cout << "  URL:       http://localhost:" << serverInfo.serverPort << "/?token=" << serverInfo.sessionToken.toStdString() << "\n";
                                    std::cout << "  Dashboard: http://localhost:" << serverInfo.serverPort << "/dev/dashboard?token=" << serverInfo.sessionToken.toStdString() << "\n";
                                } else {
                                    std::cout << "  URL:       http://localhost:" << serverInfo.serverPort << "\n";
                                }
                            } else {
                                std::cout << "  Port:      (random allocation in progress)\n";
                                std::cout << "  URL:       (pending port allocation)\n";
                            }
                            std::cout << "  Node PID:  " << serverInfo.nodePid << "\n";
                            if (serverInfo.beamPid > 0) {
                                std::cout << "  BEAM PID:  " << serverInfo.beamPid << "\n";
                            }
                            std::cout << "  Logs:      " << serverInfo.logPath.toStdString() << "\n";
                            
                            QString mcpPort = qgetenv("TAU5_MCP_PORT");
                            if (!mcpPort.isEmpty() && mcpPort != "0") {
                                std::cout << "  MCP:       Port " << mcpPort.toStdString();
                                if (qgetenv("TAU5_TIDEWAVE_ENABLED") == "true") {
                                    std::cout << " (with Tidewave)";
                                }
                                std::cout << "\n";
                            }
                            
                            std::cout << "========================================================\n";
                            std::cout << "Press Ctrl+C to stop\n" << std::flush;
                        }
                    }
                });
                return; // Wait for the timer
            }
            
            // If we have a fixed port or already got the allocated port, show immediately
            if (!serverInfoShown) {
                serverInfoShown = true;
                if (args.verbose) {
                    Tau5Logger::instance().info("OTP supervision tree ready");
                    Tau5Logger::instance().info("Server is running. Press Ctrl+C to stop.");
                } else {
                    // Print concise summary in quiet mode
                    std::cout << "\n========================================================\n";
                    std::cout << "Tau5 Server Started\n";
                    std::cout << "--------------------------------------------------------\n";
                    std::cout << "  Mode:      " << serverInfo.mode.toStdString() << "\n";
                    std::cout << "  Port:      " << serverInfo.serverPort << "\n";
                    if (serverInfo.mode == "development" && !serverInfo.sessionToken.isEmpty()) {
                        std::cout << "  URL:       http://localhost:" << serverInfo.serverPort << "/?token=" << serverInfo.sessionToken.toStdString() << "\n";
                        std::cout << "  Dashboard: http://localhost:" << serverInfo.serverPort << "/dev/dashboard?token=" << serverInfo.sessionToken.toStdString() << "\n";
                    } else {
                        std::cout << "  URL:       http://localhost:" << serverInfo.serverPort << "\n";
                    }
                    std::cout << "  Node PID:  " << serverInfo.nodePid << "\n";
                    if (serverInfo.beamPid > 0) {
                        std::cout << "  BEAM PID:  " << serverInfo.beamPid << "\n";
                    }
                    std::cout << "  Logs:      " << serverInfo.logPath.toStdString() << "\n";

                QString mcpPort = qgetenv("TAU5_MCP_PORT");
                if (!mcpPort.isEmpty() && mcpPort != "0") {
                    std::cout << "  MCP:       Port " << mcpPort.toStdString();
                    if (qgetenv("TAU5_TIDEWAVE_ENABLED") == "true") {
                        std::cout << " (with Tidewave)";
                    }
                    std::cout << "\n";
                }

                std::cout << "========================================================\n";
                std::cout << "Press Ctrl+C to stop\n\n" << std::flush;
                }
            }
        });

        // Connect standard output/error for visibility
        QObject::connect(beam.get(), &Beam::standardOutput, [&args](const QString& output) {
            // Log BEAM output to the beam category only in verbose mode
            if (args.verbose) {
                Tau5Logger::instance().log(LogLevel::Info, "beam", output);
            }
        });

        QObject::connect(beam.get(), &Beam::standardError, [&args](const QString& error) {
            // Log BEAM errors to the beam category only in verbose mode
            if (args.verbose) {
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
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&beam, &args]() {
        if (args.verbose) {
            Tau5Logger::instance().info("Shutting down Tau5 Node...");
        } else {
            std::cout << "\nShutting down Tau5 Node...\n";
        }
        if (beam) {
            beam.reset();
        }
        Tau5Common::cleanupSignalHandlers();
        if (args.verbose) {
            Tau5Logger::instance().info("Tau5 Node stopped");
        }
    });

    int result = app.exec();

    if (result == 0) {
        std::cout << "\nTau5 Node shutdown complete\n";
    }

    return result;
}