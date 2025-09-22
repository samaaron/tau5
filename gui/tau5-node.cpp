#include <iostream>
#include <memory>
#include <cstdlib>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <QTimer>
#include <QMetaObject>
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
#include "shared/qt_message_handler.h"
#include "shared/server_info.h"
#include "shared/cli_help.h"

using namespace Tau5Common;



int main(int argc, char *argv[]) {
#ifdef Q_OS_WIN
    // Set console to UTF-8 mode immediately on Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Set stdout to binary mode to prevent any text translation
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    // Enforce release settings before anything else
    Tau5CLI::enforceReleaseSettings();

// TAU5_MODE is now set via ServerConfig, not environment variables

    // Parse command line arguments
    Tau5CLI::CommonArgs args;

    for (int i = 1; i < argc; ++i) {
        const char* nextArg = (i + 1 < argc) ? argv[i + 1] : nullptr;

        if (Tau5CLI::parseSharedArg(argv[i], nextArg, i, args)) {
            if (args.hasError) {
                std::cerr << "Error: " << args.errorMessage << "\n";
                std::cout << Tau5CLI::generateHelpText(Tau5Common::BinaryType::Node, argv[0]);
                return static_cast<int>(ExitCode::INVALID_ARGUMENTS);
            }
            if (args.showHelp) {
                std::cout << Tau5CLI::generateHelpText(Tau5Common::BinaryType::Node, argv[0]);
                return 0;
            }
            if (args.showVersion) {
                std::cout << Tau5CLI::generateVersionString(Tau5Common::BinaryType::Node) << "\n";
                return 0;
            }
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            std::cout << Tau5CLI::generateHelpText(Tau5Common::BinaryType::Node, argv[0]);
            return static_cast<int>(ExitCode::INVALID_ARGUMENTS);
        }
    }

    setupConsoleOutput();
    setupSignalHandlers();

#ifdef TAU5_RELEASE_BUILD
    // In release builds, reject all development-only flags
    if (args.env == Tau5CLI::CommonArgs::Env::Dev) {
        std::cerr << "Error: Development mode is not available in release builds\n";
        return 1;
    }
    if (args.env == Tau5CLI::CommonArgs::Env::Test) {
        std::cerr << "Error: Test mode is not available in release builds\n";
        return 1;
    }
    if (args.tidewave) {
        std::cerr << "Error: Tidewave MCP server (--with-tidewave) is not available in release builds\n";
        return 1;
    }
    if (args.repl) {
        std::cerr << "Error: Elixir REPL (--with-repl) is not available in release builds\n";
        return 1;
    }
    // The --devtools flag combines multiple dev features, reject it too
    bool hasDevToolsFlag = false;
    for (int i = 1; i < argc; i++) {
        if (QString(argv[i]) == "--devtools") {
            hasDevToolsFlag = true;
            break;
        }
    }
    if (hasDevToolsFlag) {
        std::cerr << "Error: Development tools (--devtools) are not available in release builds\n";
        return 1;
    }
    // Force production settings
    args.env = Tau5CLI::CommonArgs::Env::Prod;
#endif

    // Handle dry-run after all arguments are parsed
    if (args.dryRun) {
        Tau5CLI::ServerConfig config(args, "tau5-node");
        Tau5CLI::printDryRunConfig(config);
        return 0;
    }

    // Validate no-local-endpoint restrictions
    if (args.noLocalEndpoint && args.portLocal > 0) {
        std::cerr << "Error: --no-local-endpoint conflicts with --port-local\n";
        std::cerr << "Cannot specify a local port when local endpoint is disabled\n";
        return 1;
    }

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

    Tau5CLI::ServerConfig serverConfig(args, "tau5-node");

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

        QString basePath = getServerBasePath(args.serverPath);
        
#ifndef TAU5_RELEASE_BUILD
        if (args.env == Tau5CLI::CommonArgs::Env::Prod) {
            basePath = resolveProductionServerPath(basePath, args.verbose);
        }
#endif

        // Run health check
        Tau5HealthCheck::HealthCheckConfig checkConfig;
        checkConfig.serverPath = basePath;
        checkConfig.binaryName = "tau5-node";
        checkConfig.isGui = false;
        checkConfig.verbose = args.verbose;
        checkConfig.strictMode = false;
        checkConfig.runTests = args.verbose;  // Run tests in verbose mode
        checkConfig.testPort = 0;
        checkConfig.serverConfig = &serverConfig;  // Pass server configuration

        return Tau5HealthCheck::runHealthCheck(checkConfig);
    }

    // Server configuration is already set via ServerConfig::configure() above

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
    logConfig.consoleEnabled = args.verbose;
    logConfig.consoleColors = true;
    logConfig.reuseRecentSession = false;

    logConfig.baseLogDir = Tau5Logger::getBaseLogDir();

    Tau5Logger::initialize(logConfig);

    installQtMessageHandler();

    bool isDevMode = (args.env == Tau5CLI::CommonArgs::Env::Dev);

    if (!args.verbose) {
        std::cout << getTau5Logo().toUtf8().constData();
        std::cout << "Starting Tau5 Node (Headless Mode)...\n" << std::flush;
    } else {
        Tau5Logger::instance().info(getTau5Logo());
        Tau5Logger::instance().info("Starting Tau5 Node (Headless Mode)...");
    }

    // Determine port based on mode and endpoint configuration
    quint16 port = 0;
    bool isCentralMode = (args.mode == Tau5CLI::CommonArgs::Mode::Central);
    bool hasLocalEndpoint = !args.noLocalEndpoint;

    if (isCentralMode) {
        // Central mode never uses local ports
        if (args.verbose) {
            Tau5Logger::instance().info("Central coordinator mode enabled");
        }
    } else if (!hasLocalEndpoint) {
        // Node mode with no local endpoint - don't allocate local port
        if (args.verbose) {
            Tau5Logger::instance().info("Node mode with no local endpoint");
        }
    } else {
        // Standard node mode with local endpoint
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
    }

    // Check if MCP port is available before starting (tau5-node doesn't use Chrome DevTools)
    if (args.mcp) {
        quint16 mcpPort = args.portMcp > 0 ? args.portMcp : (5550 + args.channel);
        if (!Tau5Common::isPortAvailable(mcpPort)) {
            QString errorMsg = QString("MCP port %1 is already in use").arg(mcpPort);
            if (args.verbose) {
                Tau5Logger::instance().error(errorMsg);
                Tau5Logger::instance().error("If running multiple Tau5 instances, use different --channel values (0-9)");
            } else {
                std::cerr << "Error: " << errorMsg.toStdString() << "\n";
                std::cerr << "If running multiple Tau5 instances, use different --channel values (0-9)\n";
            }
            return static_cast<int>(ExitCode::PORT_ALLOCATION_FAILED);
        }
    }

    if (args.verbose) {
        Tau5Logger::instance().info(QString("Using port: %1").arg(port));

        if (args.mcp) {
            quint16 mcpPort = args.portMcp > 0 ? args.portMcp : 5555;
            Tau5Logger::instance().info(QString("MCP endpoint enabled on port %1").arg(mcpPort));
            if (args.tidewave) {
                Tau5Logger::instance().info("Tidewave MCP server enabled");
            }
        }

        if (args.repl) {
            Tau5Logger::instance().info("Elixir REPL console enabled");
        }
    }

    QString basePath = getServerBasePath(args.serverPath);

#ifndef TAU5_RELEASE_BUILD
    if (!isDevMode) {
        basePath = resolveProductionServerPath(basePath, args.verbose);
    }
#endif

    // Check if server path is configured
    if (basePath.isEmpty()) {
        if (args.verbose) {
            Tau5Logger::instance().error("FATAL: No server path configured");
            Tau5Logger::instance().error("Please specify server location using one of:");
            Tau5Logger::instance().error("  --dev-server-path /path/to/server");
            Tau5Logger::instance().error("  TAU5_SERVER_PATH environment variable");
        } else {
            std::cerr << "FATAL: No server path configured\n";
            std::cerr << "Please specify server location using one of:\n";
            std::cerr << "  --dev-server-path /path/to/server\n";
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
            Tau5Logger::instance().error("Cannot use development mode with a release build.");
            Tau5Logger::instance().error("Release builds only support production mode.");
        } else {
            std::cerr << "Error: Cannot use development mode with a release build.\n";
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
            Tau5Logger::instance().error("Development mode requires source structure (mix.exs) but not found");
            Tau5Logger::instance().error(QString("Server path: %1").arg(basePath));
            Tau5Logger::instance().error("This appears to be a release structure. Build with release flags for production mode.");
        } else {
            std::cerr << "Error: Development mode requires source structure (mix.exs) but not found at:\n";
            std::cerr << "  " << basePath.toStdString() << "\n";
            std::cerr << "This appears to be a release structure. Build with release flags for production mode.\n";
        }
        return static_cast<int>(ExitCode::INVALID_ARGUMENTS);
    }

    if (!isDevMode && !hasReleaseStructure) {
        // Only error if we also don't have source structure
        if (!hasSourceStructure) {
            if (args.verbose) {
                Tau5Logger::instance().error("Production mode requires release structure but server directory is invalid");
                Tau5Logger::instance().error(QString("Server path: %1").arg(basePath));
            } else {
                std::cerr << "Error: Production mode requires release structure but server directory is invalid:\n";
                std::cerr << "  " << basePath.toStdString() << "\n";
            }
            return static_cast<int>(ExitCode::SERVER_DIR_NOT_FOUND);
        }
        // If we have source structure but no release, give helpful message
        if (args.verbose) {
            Tau5Logger::instance().error("Production mode requires release structure (bin/tau5) but not found");
            Tau5Logger::instance().error(QString("To create a production release:"));
            Tau5Logger::instance().error(QString("  cd %1").arg(basePath));
            Tau5Logger::instance().error("  MIX_ENV=prod mix deps.get --only prod");
            Tau5Logger::instance().error("  MIX_ENV=prod mix compile");
            Tau5Logger::instance().error("  MIX_ENV=prod mix release");
            Tau5Logger::instance().error("Or build without release flags for development mode.");
        } else {
            std::cerr << "Error: Production mode requires release structure (bin/tau5) but not found.\n";
            std::cerr << "To create a production release:\n";
            std::cerr << "  cd " << basePath.toStdString() << "\n";
            std::cerr << "  MIX_ENV=prod mix deps.get --only prod\n";
            std::cerr << "  MIX_ENV=prod mix compile\n";
            std::cerr << "  MIX_ENV=prod mix release\n";
            std::cerr << "Or build without release flags for development mode.\n";
        }
        return static_cast<int>(ExitCode::INVALID_ARGUMENTS);
    }
#endif

    // Create BEAM instance
    std::shared_ptr<Beam> beam;

    // Track service availability for summary
    ServerInfo serverInfo;
    serverInfo.binaryType = BinaryType::Node;
#ifdef TAU5_RELEASE_BUILD
    serverInfo.isDevBuild = false;
#else
    serverInfo.isDevBuild = true;
#endif
    serverInfo.serverPort = port;
    serverInfo.publicPort = args.portPublic;
    serverInfo.mode = getServerModeString(isDevMode);
    serverInfo.hasLocalEndpoint = !args.noLocalEndpoint;
    serverInfo.friendToken = QString::fromStdString(args.friendToken);

    // Get Node PID
    #ifndef Q_OS_WIN
    serverInfo.nodePid = getpid();
    #else
    serverInfo.nodePid = _getpid();
    #endif

    // Get log path from logger
    serverInfo.logPath = Tau5Logger::instance().currentSessionPath();
    
    serverInfo.channel = args.channel;

    if (args.mcp) {
        serverInfo.hasMcpEndpoint = true;
        serverInfo.mcpPort = serverConfig.getMcpPort();
        serverInfo.hasTidewave = args.tidewave;
    }

    serverInfo.hasRepl = args.repl;
    serverInfo.hasDebugPane = args.debugPane;
    
    // Simple progress dots timer
    QTimer* dotsTimer = nullptr;
    if (!args.verbose) {
        std::cout << "Starting BEAM server" << std::flush;
        dotsTimer = new QTimer(&app);
        QObject::connect(dotsTimer, &QTimer::timeout, []() {
            std::cout << "." << std::flush;
        });
        dotsTimer->start(500);
    }

    // Defer BEAM creation until event loop is running using Qt's event queue
    QMetaObject::invokeMethod(&app, [&app, &beam, basePath, port, &args, &serverConfig, &serverInfo, dotsTimer]() {
        if (args.verbose) {
            Tau5Logger::instance().info("Starting BEAM server...");
        }

        // Create Beam instance with server configuration
        beam = std::make_shared<Beam>(&app, serverConfig, basePath, Config::APP_NAME,
                                     Config::APP_VERSION, port);

        // Get session token from beam
        serverInfo.sessionToken = beam->getSessionToken();

        // Track whether we've shown the server info yet
        bool serverInfoShown = false;
        QTimer* portTimeoutTimer = nullptr;

        // Set up timeout for port allocation (only for random ports)
        if (port == 0 && !args.noLocalEndpoint) {
            portTimeoutTimer = new QTimer(&app);
            portTimeoutTimer->setSingleShot(true);
            QObject::connect(portTimeoutTimer, &QTimer::timeout, [&serverInfo, &args, &serverInfoShown, &beam]() {
                if (!serverInfoShown && serverInfo.otpReady && serverInfo.serverPort == 0) {
                    serverInfoShown = true;
                    
                    if (beam) {
                        serverInfo.beamPid = beam->getBeamPid();
                    }
                    
                    QString infoString = generateServerInfoString(serverInfo, args.verbose);
                    if (args.verbose) {
                        Tau5Logger::instance().warning("Port allocation timed out, showing info with unavailable port");
                        Tau5Logger::instance().info(infoString);
                    } else {
                        std::cout << infoString.toStdString() << "\n" << std::flush;
                    }
                }
            });
        }

        // Connect to actualPortAllocated signal to update the port when we get it
        QObject::connect(beam.get(), &Beam::actualPortAllocated, [&serverInfo, &args, &serverInfoShown, &beam, &portTimeoutTimer](quint16 actualPort) {
        if (actualPort > 0) {
            serverInfo.serverPort = actualPort;
            
            if (portTimeoutTimer) {
                portTimeoutTimer->stop();
                portTimeoutTimer->deleteLater();
                portTimeoutTimer = nullptr;
            }
            
            if (args.verbose) {
                Tau5Logger::instance().info(QString("Server allocated port: %1").arg(actualPort));
            }
            // Try to show server info now that we have the port
            if (!serverInfoShown) {
                // Check if we have all the info we need
                if (serverInfo.otpReady && (!serverInfo.hasLocalEndpoint || serverInfo.serverPort > 0)) {
                    serverInfoShown = true;
                    
                    if (beam) {
                        serverInfo.beamPid = beam->getBeamPid();
                    }
                    
                    QString infoString = generateServerInfoString(serverInfo, args.verbose);
                    if (args.verbose) {
                        Tau5Logger::instance().info("OTP supervision tree ready");
                        Tau5Logger::instance().info(infoString);
                    } else {
                        std::cout << infoString.toStdString() << "\n" << std::flush;
                    }
                }
            }
        }
        });

        // Connect to OTP ready signal
        QObject::connect(beam.get(), &Beam::otpReady, [&args, &serverInfo, &beam, &serverInfoShown, dotsTimer, portTimeoutTimer, port]() {
            serverInfo.otpReady = true;
            
            // Stop the dots timer
            if (dotsTimer) {
                dotsTimer->stop();
                dotsTimer->deleteLater();
                if (!args.verbose) {
                    std::cout << " done\n" << std::flush;
                }
            }
            
            if (portTimeoutTimer && port == 0 && !args.noLocalEndpoint && serverInfo.serverPort == 0) {
                portTimeoutTimer->start(1000); // 1 second timeout for port allocation
            }

            // Get BEAM PID and session token when ready
            if (beam) {
                serverInfo.beamPid = beam->getBeamPid();
                serverInfo.sessionToken = beam->getSessionToken();
                // Also update port in case it was allocated
                quint16 actualPort = beam->getPort();
                if (actualPort > 0) {
                    serverInfo.serverPort = actualPort;
                }
            }
            
            // Try to show server info now that OTP is ready
            if (!serverInfoShown) {
                // Check if we have all the info we need
                if (serverInfo.otpReady && (!serverInfo.hasLocalEndpoint || serverInfo.serverPort > 0)) {
                    serverInfoShown = true;
                    
                    if (beam) {
                        serverInfo.beamPid = beam->getBeamPid();
                    }
                    
                    QString infoString = generateServerInfoString(serverInfo, args.verbose);
                    if (args.verbose) {
                        Tau5Logger::instance().info("OTP supervision tree ready");
                        Tau5Logger::instance().info(infoString);
                    } else {
                        std::cout << infoString.toStdString() << "\n" << std::flush;
                    }
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
    }, Qt::QueuedConnection);

    // Ensure BEAM is terminated before the app fully exits
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&beam, &args]() {
        if (args.verbose) {
            Tau5Logger::instance().info("Shutting down Tau5 (politely and patiently)... ");
        } else {
            std::cout << "\nShutting down Tau5 (politely and patiently)... " << std::flush;
        }
        // Normal shutdown - let the destructor do its job
        if (beam) {
            beam.reset();
        }
        
        // Clean up signal handlers
        Tau5Common::cleanupSignalHandlers();
        
        if (args.verbose) {
            Tau5Logger::instance().info("Tau5 Node stopped");
        } else {
            std::cout << " done\n" << std::flush;
        }
    });

    int result = app.exec();

    if (result == 0) {
        std::cout << "\nTau5 Node shutdown complete\n";
    }

    return result;
}