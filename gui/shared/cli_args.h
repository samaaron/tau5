#ifndef TAU5_CLI_ARGS_H
#define TAU5_CLI_ARGS_H

#include <cstring>
#include <string>
#include <cstdlib>
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <QtCore/QtGlobal>
#include <QtCore/QCoreApplication>

namespace Tau5CLI {

struct CommonArgs {
    // Runtime environment (controls MIX_ENV)
    enum class Env {
        Default,  // Not specified, will use build default
        Dev,      // Development environment
        Prod,     // Production environment
        Test      // Test environment
    };
    #ifdef TAU5_RELEASE_BUILD
    Env env = Env::Prod;  // Release builds default to production
    #else
    Env env = Env::Dev;   // Development builds default to development
    #endif
    bool serverModeExplicitlySet = false; // Track if server mode was explicitly set via --dev-with-release-server

    // Deployment mode override (tau5-node only, controls TAU5_MODE)
    enum class Mode {
        Default,  // Not specified, binary sets its own default
        Node,     // Local headless server with NIFs
        Central   // Public coordinator without NIFs
    };
    Mode mode = Mode::Default;

    // Channel configuration (modifies default ports)
    int channel = 0;           // Channel number 0-9, default 0

    // Port configuration
    quint16 portLocal = 0;    // Local web UI port (0 = random)
    quint16 portPublic = 0;   // Public endpoint port (0 = disabled)
    quint16 portMcp = 0;      // MCP services port (0 = use channel-based default 555X when enabled)
    quint16 portChrome = 0;   // Chrome DevTools port (0 = use channel-based default 922X when enabled)
    quint16 portHeartbeat = 0; // Heartbeat UDP port (0 = random)
    
    // Quick setup flags
    bool devtools = false;     // Convenience flag: enables dev mode + MCP + Chrome DevTools + Tidewave
    
    // Optional features (default: disabled, flag enables)
    bool mcp = false;              // Enable MCP endpoint
    bool tidewave = false;         // Tidewave on MCP endpoint (implies mcp)
    bool chromeDevtools = false;   // Enable Chrome DevTools
    bool repl = false;             // Elixir REPL (dev mode only)
    bool verbose = false;          // Verbose logging
    bool debugPane = true;         // Debug pane (tau5 only, default enabled)
    bool allowRemoteAccess = false; // Allow remote asset/site access (debugging only)
    
    // NIF control (default: enabled, --no-* disables)
    bool noMidi = false;           // Disable MIDI support
    bool noLink = false;           // Disable Ableton Link support
    bool noDiscovery = false;      // Disable network discovery
    bool noNifs = false;           // Disable all NIFs
    bool noLocalEndpoint = false;  // Disable local endpoint (tau5-node only)
    
    // Other
    bool check = false;            // Verify installation
    bool showHelp = false;         // Show help
    bool showVersion = false;      // Show version
    bool dryRun = false;           // Dry run - show configuration without starting
    
    // Path overrides
    std::string serverPath;        // Override server path (instead of using TAU5_SERVER_PATH env var)
    
    // Friend authentication
    std::string friendToken;       // Friend authentication token for public endpoint
    
    // Error handling
    bool hasError = false;
    std::string errorMessage;
};

// Immutable server configuration generated from CommonArgs
class ServerConfig {
public:
    // Constructor that resolves all defaults and creates immutable config
    explicit ServerConfig(const CommonArgs& args, const std::string& binaryType = "gui")
        : m_args(args), m_binaryType(binaryType) {

        // Resolve environment default
        if (m_args.env == CommonArgs::Env::Default) {
#ifdef TAU5_RELEASE_BUILD
            m_args.env = CommonArgs::Env::Prod;
#else
            m_args.env = CommonArgs::Env::Dev;
#endif
        }

        // Resolve mode default based on binary type
        if (m_args.mode == CommonArgs::Mode::Default) {
            if (binaryType == "node" || binaryType == "tau5-node") {
                // tau5-node defaults to node mode
                m_resolvedMode = "node";
            } else {
                // tau5/gui defaults to gui mode
                m_resolvedMode = "gui";
            }
        } else {
            m_resolvedMode = (m_args.mode == CommonArgs::Mode::Node) ? "node" : "central";
        }

        // Calculate derived values
        m_mcpPort = m_args.portMcp > 0 ? m_args.portMcp : (5550 + m_args.channel);
        m_chromePort = m_args.portChrome > 0 ? m_args.portChrome : (9220 + m_args.channel);
    }

    // Getters for configuration values
    const CommonArgs& getArgs() const { return m_args; }
    std::string getBinaryType() const { return m_binaryType; }
    std::string getResolvedMode() const { return m_resolvedMode; }
    quint16 getMcpPort() const { return m_mcpPort; }
    quint16 getChromePort() const { return m_chromePort; }

    // Get environment string for MIX_ENV
    std::string getMixEnv() const {
        switch (m_args.env) {
            case CommonArgs::Env::Dev: return "dev";
            case CommonArgs::Env::Prod: return "prod";
            case CommonArgs::Env::Test: return "test";
            default: return "dev";
        }
    }

    // Generate environment variables as a map
    std::map<std::string, std::string> generateEnvironmentVars() const {
        std::map<std::string, std::string> env;

        // MIX_ENV
        env["MIX_ENV"] = getMixEnv();

        // TAU5_MODE
        env["TAU5_MODE"] = m_resolvedMode;

        // Ports
        if (m_args.portPublic > 0) {
            env["TAU5_PUBLIC_PORT"] = std::to_string(m_args.portPublic);
            env["TAU5_PUBLIC_ENDPOINT_ENABLED"] = "true";
        }
        if (m_args.portHeartbeat > 0) {
            env["TAU5_HEARTBEAT_PORT"] = std::to_string(m_args.portHeartbeat);
        }

        // MCP
        env["TAU5_MCP_ENABLED"] = m_args.mcp ? "true" : "false";
        env["TAU5_MCP_PORT"] = std::to_string(m_mcpPort);

        // Development features (only in dev builds)
#ifndef TAU5_RELEASE_BUILD
        if (m_args.tidewave) {
            env["TAU5_TIDEWAVE_ENABLED"] = "true";
        }
        if (m_args.repl) {
            env["TAU5_ELIXIR_REPL_ENABLED"] = "true";
        }
#endif

        // NIF configuration
        if (m_args.noMidi) env["TAU5_MIDI_ENABLED"] = "false";
        if (m_args.noLink) env["TAU5_LINK_ENABLED"] = "false";
        if (m_args.noDiscovery) env["TAU5_DISCOVERY_ENABLED"] = "false";

        // Other settings
        if (m_args.verbose) env["TAU5_VERBOSE"] = "true";
        if (m_args.noLocalEndpoint) env["TAU5_NO_LOCAL_ENDPOINT"] = "true";

        // Friend authentication
        if (!m_args.friendToken.empty()) {
            env["TAU5_FRIEND_MODE"] = "true";
            env["TAU5_FRIEND_TOKEN"] = m_args.friendToken;
            env["TAU5_FRIEND_REQUIRE_TOKEN"] = "true";
        }

        return env;
    }

private:
    CommonArgs m_args;  // Copy of args with defaults resolved
    std::string m_binaryType;
    std::string m_resolvedMode;
    quint16 m_mcpPort;
    quint16 m_chromePort;
};

// Helper function to generate a secure random token
inline std::string generateSecureToken(size_t length = 32) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const size_t charset_size = sizeof(charset) - 1;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charset_size - 1);
    
    std::string token;
    token.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        token += charset[dis(gen)];
    }
    
    return token;
}

// Helper function to parse port argument
// Returns true if there was an error, false if successful
inline bool parsePort(const char* nextArg, int& i, quint16& portValue, CommonArgs& args, const char* argName) {
    if (nextArg != nullptr) {
        // Check if it's a valid number first
        char* endPtr;
        long port = std::strtol(nextArg, &endPtr, 10);
        if (*endPtr != '\0' || endPtr == nextArg) {
            // Not a valid number
            args.hasError = true;
            args.errorMessage = std::string(argName) + " must be a valid number";
            return true;
        }
        // Allow 0 to explicitly disable
        if (port < 0 || port > 65535) {
            args.hasError = true;
            args.errorMessage = std::string(argName) + " must be between 0 and 65535";
            return true;
        }
        portValue = static_cast<quint16>(port);
        i++; // Consume next arg
    } else {
        args.hasError = true;
        args.errorMessage = std::string(argName) + " requires a port number";
        return true;
    }
    return false;
}

// Parse shared command-line arguments used by both tau5 and tau5-node
// Returns true if the argument was recognized (whether successful or not)
// Check args.hasError to see if there was an error processing the argument
inline bool parseSharedArg(const char* arg, const char* nextArg, int& i, CommonArgs& args) {
    // Quick development setup
    if (std::strcmp(arg, "--devtools") == 0) {
        args.devtools = true;
        // This implies several other flags
#ifdef TAU5_RELEASE_BUILD
        // Release builds force prod environment
        args.env = CommonArgs::Env::Prod;
        // Other devtools flags are ignored in release
#else
        // Only set server to dev mode if not explicitly set to prod by --dev-with-release-server
        if (!args.serverModeExplicitlySet) {
            args.env = CommonArgs::Env::Dev;
        }
        // GUI dev features are always enabled with --devtools
        args.mcp = true;
        args.tidewave = true;
        args.chromeDevtools = true;
        args.repl = true;
#endif
        return true;
    }
    // Deployment mode override
    else if (std::strcmp(arg, "--mode-node") == 0) {
        args.mode = CommonArgs::Mode::Node;
        return true;
    } else if (std::strcmp(arg, "--mode-central") == 0) {
        args.mode = CommonArgs::Mode::Central;
        return true;
    }
    // Channel configuration
    else if (std::strcmp(arg, "--channel") == 0) {
        if (nextArg != nullptr) {
            char* endPtr;
            long channelNum = std::strtol(nextArg, &endPtr, 10);
            if (*endPtr != '\0' || endPtr == nextArg) {
                args.hasError = true;
                args.errorMessage = "--channel must be a valid number";
                return true;
            }
            if (channelNum < 0 || channelNum > 9) {
                args.hasError = true;
                args.errorMessage = "--channel must be between 0 and 9";
                return true;
            }
            args.channel = static_cast<int>(channelNum);
            i++; // Consume next arg
        } else {
            args.hasError = true;
            args.errorMessage = "--channel requires a number between 0 and 9";
        }
        return true;
    }
    // Port configuration
    else if (std::strcmp(arg, "--port-local") == 0) {
        parsePort(nextArg, i, args.portLocal, args, "--port-local");
        return true;
    } else if (std::strcmp(arg, "--port-public") == 0) {
        parsePort(nextArg, i, args.portPublic, args, "--port-public");
        return true;
    } else if (std::strcmp(arg, "--port-mcp") == 0) {
        parsePort(nextArg, i, args.portMcp, args, "--port-mcp");
        if (!args.hasError) args.mcp = true;
        return true;
    } else if (std::strcmp(arg, "--dev-port-chrome-cdp") == 0) {
        parsePort(nextArg, i, args.portChrome, args, "--dev-port-chrome-cdp");
        if (!args.hasError) args.chromeDevtools = true;
        return true;
    } else if (std::strcmp(arg, "--port-heartbeat") == 0) {
        parsePort(nextArg, i, args.portHeartbeat, args, "--port-heartbeat");
        return true;
    }
    // Optional features (enable with flag)
    else if (std::strcmp(arg, "--mcp") == 0) {
        args.mcp = true;
        return true;
    } else if (std::strcmp(arg, "--dev-tidewave") == 0) {
        args.tidewave = true;
        args.mcp = true;
        return true;
    } else if (std::strcmp(arg, "--dev-chrome-cdp") == 0) {
        args.chromeDevtools = true;
        return true;
    } else if (std::strcmp(arg, "--dev-repl") == 0) {
        args.repl = true;
        return true;
    } else if (std::strcmp(arg, "--verbose") == 0) {
        args.verbose = true;
        return true;
    } else if (std::strcmp(arg, "--debug-pane") == 0) {
        args.debugPane = true;
        return true;
    } else if (std::strcmp(arg, "--dev-allow-remote-access") == 0) {
        args.allowRemoteAccess = true;
        return true;
    }
    // Disable features
    else if (std::strcmp(arg, "--no-midi") == 0) {
        args.noMidi = true;
        return true;
    } else if (std::strcmp(arg, "--no-link") == 0) {
        args.noLink = true;
        return true;
    } else if (std::strcmp(arg, "--no-discovery") == 0) {
        args.noDiscovery = true;
        return true;
    } else if (std::strcmp(arg, "--no-nifs") == 0) {
        args.noNifs = true;
        args.noMidi = true;
        args.noLink = true;
        args.noDiscovery = true;
        return true;
    } else if (std::strcmp(arg, "--dev-no-debug-pane") == 0) {
        args.debugPane = false;
        return true;
    } else if (std::strcmp(arg, "--no-local-endpoint") == 0) {
        args.noLocalEndpoint = true;
        return true;
    }
    // Path configuration
    else if (std::strcmp(arg, "--dev-server-path") == 0) {
        if (nextArg != nullptr) {
            args.serverPath = nextArg;
            i++;
        } else {
            args.hasError = true;
            args.errorMessage = "--dev-server-path requires a path";
        }
        return true;
    }
    // Friend authentication
    else if (std::strcmp(arg, "--friend-token") == 0) {
        // Check if next argument exists and is not another flag
        if (nextArg != nullptr && nextArg[0] != '-') {
            // User provided a specific token
            args.friendToken = nextArg;
            i++;
        } else {
            // No token provided, generate a secure random one
            args.friendToken = generateSecureToken();
        }
        
        // When friend token is specified, implicitly enable friend mode
        // and the public endpoint (if not already set)
        if (args.portPublic == 0) {
            // Will use default port from server (7005 or auto-find)
            args.portPublic = 7005;
        }
        return true;
    }
    // Server mode override
    else if (std::strcmp(arg, "--dev-with-release-server") == 0) {
        args.env = CommonArgs::Env::Prod;
        args.serverModeExplicitlySet = true;
        return true;
    }
    // Other
    else if (std::strcmp(arg, "--check") == 0) {
        args.check = true;
        return true;
    } else if (std::strcmp(arg, "--dry-run") == 0) {
        args.dryRun = true;
        return true;
    } else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
        args.showHelp = true;
        return true;
    } else if (std::strcmp(arg, "--version") == 0) {
        args.showVersion = true;
        return true;
    }
    
    return false;
}

// Validate arguments for conflicts and dependencies
inline bool validateArguments(CommonArgs& args) {
    if (args.devtools) {
        return true;
    }
    
#ifdef TAU5_RELEASE_BUILD
    // Release builds have already forced prod environment during parsing
    // Double-check here just in case
    if (args.env != CommonArgs::Env::Prod && args.env != CommonArgs::Env::Default) {
        args.env = CommonArgs::Env::Prod;
    }
    
    if (args.repl) {
        args.hasError = true;
        args.errorMessage = "--dev-repl only works in development builds";
        return false;
    }
#endif
    
    if (args.portLocal > 0 && args.portPublic > 0 && args.portLocal == args.portPublic) {
        args.hasError = true;
        args.errorMessage = "Local and public ports cannot be the same";
        return false;
    }
    
    if (args.portLocal > 0 && args.portMcp > 0 && args.portLocal == args.portMcp) {
        args.hasError = true;
        args.errorMessage = "Local and MCP ports cannot be the same";
        return false;
    }
    
    if (args.portPublic > 0 && args.portMcp > 0 && args.portPublic == args.portMcp) {
        args.hasError = true;
        args.errorMessage = "Public and MCP ports cannot be the same";
        return false;
    }
    
    if (args.portChrome > 0 && (args.portChrome == args.portLocal || 
                                 args.portChrome == args.portPublic || 
                                 args.portChrome == args.portMcp)) {
        args.hasError = true;
        args.errorMessage = "Chrome DevTools port conflicts with another port";
        return false;
    }
    
    if (args.portHeartbeat > 0) {
        if (args.portHeartbeat == args.portLocal || 
            args.portHeartbeat == args.portPublic || 
            args.portHeartbeat == args.portMcp ||
            args.portHeartbeat == args.portChrome) {
            args.hasError = true;
            args.errorMessage = "Heartbeat port conflicts with another port";
            return false;
        }
    }
    
    if (args.tidewave && !args.mcp) {
        args.mcp = true;
    }
    
#ifdef TAU5_RELEASE_BUILD
    if (args.tidewave) {
        args.hasError = true;
        args.errorMessage = "--dev-tidewave only works in development builds";
        return false;
    }
#endif
    
    auto validatePort = [&](quint16 port, const char* name) {
        if (port > 65535) {
            args.hasError = true;
            args.errorMessage = std::string(name) + " port number is invalid";
            return false;
        }
        return true;
    };
    
    if (!validatePort(args.portLocal, "Local")) return false;
    if (!validatePort(args.portPublic, "Public")) return false;
    if (!validatePort(args.portMcp, "MCP")) return false;
    if (!validatePort(args.portChrome, "Chrome DevTools")) return false;
    if (!validatePort(args.portHeartbeat, "Heartbeat")) return false;
    
    if (args.portMcp > 0 && !args.mcp) {
        args.mcp = true;
    }
    
    if (args.portChrome > 0 && !args.chromeDevtools) {
        args.chromeDevtools = true;
    }
    
    if (args.mode == CommonArgs::Mode::Central) {
        if (!args.noNifs) {
            args.noMidi = true;
            args.noLink = true;
            args.noDiscovery = true;
            args.noNifs = true;
        }
    }
    
    return true;
}

// Apply environment variables based on parsed arguments
inline void applyEnvironmentVariables(const CommonArgs& args, const char* targetOverride = nullptr) {
    // Set MIX_ENV based on environment setting
    // Note: Release builds have already forced args.env to Prod in validateArguments
    switch (args.env) {
        case CommonArgs::Env::Dev:
            qputenv("MIX_ENV", "dev");
            break;
        case CommonArgs::Env::Prod:
            qputenv("MIX_ENV", "prod");
            break;
        case CommonArgs::Env::Test:
            qputenv("MIX_ENV", "test");
            break;
        case CommonArgs::Env::Default:
            // Fall back to build-time defaults
#ifdef TAU5_RELEASE_BUILD
            qputenv("MIX_ENV", "prod");
#else
            qputenv("MIX_ENV", "dev");
#endif
            break;
    }
    
    // Set TAU5_MODE based on deployment target
    // Priority: explicit mode flag > targetOverride > default
    switch (args.mode) {
        case CommonArgs::Mode::Node:
            qputenv("TAU5_MODE", "node");
            break;
        case CommonArgs::Mode::Central:
            qputenv("TAU5_MODE", "central");
            break;
        case CommonArgs::Mode::Default:
            // Use targetOverride if provided, otherwise system default
            if (targetOverride) {
                qputenv("TAU5_MODE", targetOverride);
            }
            break;
    }
    
    // Set port configuration
    if (args.portLocal > 0) {
        qputenv("TAU5_LOCAL_PORT", std::to_string(args.portLocal).c_str());
    }
    if (args.portPublic > 0) {
        qputenv("TAU5_PUBLIC_PORT", std::to_string(args.portPublic).c_str());
        // If public endpoint is enabled, also enable public endpoint in the server
        qputenv("TAU5_PUBLIC_ENDPOINT_ENABLED", "true");
    }
    if (args.portHeartbeat > 0) {
        qputenv("TAU5_HEARTBEAT_PORT", std::to_string(args.portHeartbeat).c_str());
    }
    
    // Friend authentication configuration
    if (!args.friendToken.empty()) {
        qputenv("TAU5_FRIEND_MODE", "true");
        qputenv("TAU5_FRIEND_TOKEN", args.friendToken.c_str());
    }
    
    // MCP configuration
    // Always set the port (for configuration/display purposes)
    // Use specified port or channel-based default (555X where X is channel)
    quint16 mcpPort = args.portMcp > 0 ? args.portMcp : (5550 + args.channel);
    qputenv("TAU5_MCP_PORT", std::to_string(mcpPort).c_str());

    if (args.mcp) {
        qputenv("TAU5_MCP_ENABLED", "true");
    } else {
        // Explicitly disable MCP to prevent Elixir server from using defaults
        qputenv("TAU5_MCP_ENABLED", "false");
    }

    // Chrome DevTools configuration
    if (args.chromeDevtools) {
        qputenv("TAU5_DEVTOOLS_ENABLED", "true");
        // Use specified port or channel-based default (922X where X is channel)
        quint16 chromePort = args.portChrome > 0 ? args.portChrome : (9220 + args.channel);
        qputenv("TAU5_DEVTOOLS_PORT", std::to_string(chromePort).c_str());
    } else {
        // Explicitly disable DevTools
        qputenv("TAU5_DEVTOOLS_ENABLED", "false");
    }
    
    // Tidewave configuration (only in dev builds)
    if (args.tidewave) {
#ifndef TAU5_RELEASE_BUILD
        qputenv("TAU5_TIDEWAVE_ENABLED", "true");
#endif
        // Silently ignored in release builds
    }
    
    // NIF control
    if (args.noMidi) {
        qputenv("TAU5_MIDI_ENABLED", "false");
    }
    if (args.noLink) {
        qputenv("TAU5_LINK_ENABLED", "false");
    }
    if (args.noDiscovery) {
        qputenv("TAU5_DISCOVERY_ENABLED", "false");
    }
    
    // Local endpoint control (tau5-node only)
    if (args.noLocalEndpoint) {
        qputenv("TAU5_NO_LOCAL_ENDPOINT", "true");
    }
    
    // Dev tools
    if (args.repl) {
#ifndef TAU5_RELEASE_BUILD
        // Elixir REPL only works in dev builds for security
        qputenv("TAU5_ELIXIR_REPL_ENABLED", "true");
#endif
        // Silently ignored in release builds
    }
    if (args.verbose) {
        qputenv("TAU5_VERBOSE", "true");
    }
}

// Enforce safe environment settings for release builds
inline void enforceReleaseSettings() {
#ifdef TAU5_RELEASE_BUILD
    // Override any dangerous environment variables
    // Force production mode
    qputenv("MIX_ENV", "prod");
    
    // Disable all development features
    qputenv("TAU5_ELIXIR_REPL_ENABLED", "false");
    qputenv("TAU5_TIDEWAVE_ENABLED", "false");
    qputenv("TAU5_DEV_MCP_ENABLED", "false");
    qputenv("TAU5_ENABLE_DEV_MCP", "false");
    qputenv("TAU5_GUI_DEV_MCP_ENABLED", "false");
    
    // Ensure console is disabled
    qputenv("TAU5_CONSOLE_ENABLED", "false");
    
    // Clear any server path overrides (use embedded server)
    qunsetenv("TAU5_SERVER_PATH");
    
    // Note: TAU5_MODE should be set by the specific executable (gui or node)
#endif
}

// Function to generate dry-run configuration string
inline std::string generateDryRunConfig(const ServerConfig& config) {
    const CommonArgs& args = config.getArgs();
    std::string binaryType = config.getBinaryType();
    std::ostringstream oss;
    oss << "\n========================================\n";
    oss << "Tau5 Configuration (--dry-run)\n";
    oss << "========================================\n\n";

    oss << "Binary Type: " << binaryType << "\n\n";

    oss << "Server Configuration:\n";
    oss << "  Environment: " << (args.env == CommonArgs::Env::Dev ? "Development" : "Production") << "\n";
    if (!args.serverPath.empty()) {
        oss << "  Server Path: " << args.serverPath << "\n";
    } else {
        const char* envPath = std::getenv("TAU5_SERVER_PATH");
        if (envPath) {
            oss << "  Server Path: " << envPath << " (from TAU5_SERVER_PATH)\n";
        } else {
            oss << "  Server Path: <embedded server>\n";
        }
    }
    oss << "\n";

    oss << "Features Enabled:\n";
    oss << "  MCP Endpoint: " << (args.mcp ? "Yes" : "No") << "\n";
    if (args.mcp) {
        oss << "    MCP Port: " << (args.portMcp > 0 ? std::to_string(args.portMcp) : std::to_string(5550 + args.channel)) << "\n";
    }
    oss << "  Verbose Logging: " << (args.verbose ? "Yes" : "No") << "\n";
    if (!args.friendToken.empty()) {
        oss << "  Friend Token: " << (args.friendToken.length() > 8 ? args.friendToken.substr(0, 8) + "..." : args.friendToken) << "\n";
    }
#ifndef TAU5_RELEASE_BUILD
    oss << "  Dev Tidewave: " << (args.tidewave ? "Yes" : "No") << "\n";
    oss << "  Dev Chrome CDP: " << (args.chromeDevtools ? "Yes" : "No") << "\n";
    if (args.chromeDevtools) {
        oss << "    CDP Port: " << (args.portChrome > 0 ? std::to_string(args.portChrome) : std::to_string(9220 + args.channel)) << "\n";
    }
    oss << "  Dev REPL: " << (args.repl ? "Yes" : "No") << "\n";
    oss << "  Debug Pane: " << (args.debugPane ? "Yes" : "No") << "\n";
    oss << "  Allow Remote Access: " << (args.allowRemoteAccess ? "Yes" : "No") << "\n";
#endif
    oss << "\n";

    oss << "Features Disabled:\n";
    oss << "  MIDI: " << (args.noMidi ? "Disabled" : "Enabled") << "\n";
    oss << "  Ableton Link: " << (args.noLink ? "Disabled" : "Enabled") << "\n";
    oss << "  Network Discovery: " << (args.noDiscovery ? "Disabled" : "Enabled") << "\n";
    if (binaryType == "tau5-node" || binaryType == "node") {
        oss << "  Local Endpoint: " << (args.noLocalEndpoint ? "Disabled" : "Enabled") << "\n";
    }
    oss << "\n";

    oss << "Port Configuration:\n";
    oss << "  Channel: " << args.channel << "\n";
    oss << "  Local Port: " << (args.portLocal > 0 ? std::to_string(args.portLocal) : "random") << "\n";
    oss << "  Public Port: " << (args.portPublic > 0 ? std::to_string(args.portPublic) : "disabled") << "\n";
    oss << "  Heartbeat Port: " << (args.portHeartbeat > 0 ? std::to_string(args.portHeartbeat) : "random") << "\n";

    if ((binaryType == "tau5-node" || binaryType == "node") && args.mode != CommonArgs::Mode::Default) {
        oss << "\nDeployment Mode:\n";
        oss << "  Mode: " << (args.mode == CommonArgs::Mode::Node ? "Node" : "Central") << "\n";
    }

    oss << "\nServer Configuration (from parsed args):\n";

    // MIX_ENV
    oss << "  MIX_ENV: ";
    switch (args.env) {
        case CommonArgs::Env::Dev:
            oss << "dev";
            break;
        case CommonArgs::Env::Prod:
            oss << "prod";
            break;
        case CommonArgs::Env::Test:
            oss << "test";
            break;
        case CommonArgs::Env::Default:
#ifdef TAU5_RELEASE_BUILD
            oss << "prod (default)";
#else
            oss << "dev (default)";
#endif
            break;
    }
    oss << "\n";

    // TAU5_MODE
    oss << "  TAU5_MODE: ";
    if (args.mode == CommonArgs::Mode::Node) {
        oss << "node";
    } else if (args.mode == CommonArgs::Mode::Central) {
        oss << "central";
    } else {
        // Default based on binary type
        if (binaryType == "tau5-node" || binaryType == "node") {
            oss << "node (default)";
        } else {
            oss << "gui (default)";
        }
    }
    oss << "\n";

    // MCP configuration
    oss << "  TAU5_MCP_ENABLED: " << (args.mcp ? "true" : "false") << "\n";
    if (args.mcp || args.portMcp > 0) {
        quint16 mcpPort = args.portMcp > 0 ? args.portMcp : (5550 + args.channel);
        oss << "  TAU5_MCP_PORT: " << mcpPort << "\n";
    }

    // Chrome CDP configuration (GUI only)
    if ((binaryType == "tau5-gui" || binaryType == "gui") && args.chromeDevtools) {
        quint16 chromePort = args.portChrome > 0 ? args.portChrome : (9220 + args.channel);
        oss << "  Chrome DevTools Port: " << chromePort << " (GUI-internal)\n";
    }

#ifndef TAU5_RELEASE_BUILD
    // Development features
    if (args.tidewave) {
        oss << "  TAU5_TIDEWAVE_ENABLED: true\n";
    }
    if (args.repl) {
        oss << "  TAU5_ELIXIR_REPL_ENABLED: true\n";
    }
#endif

    // NIF configuration
    if (args.noMidi) oss << "  TAU5_MIDI_ENABLED: false\n";
    if (args.noLink) oss << "  TAU5_LINK_ENABLED: false\n";
    if (args.noDiscovery) oss << "  TAU5_DISCOVERY_ENABLED: false\n";

    // Other settings
    if (args.verbose) oss << "  TAU5_VERBOSE: true\n";
    if (args.portPublic > 0) oss << "  TAU5_PUBLIC_PORT: " << args.portPublic << "\n";
    if (!args.friendToken.empty()) {
        oss << "  TAU5_FRIEND_MODE: true\n";
        oss << "  TAU5_FRIEND_TOKEN: [set]\n";
        oss << "  TAU5_FRIEND_REQUIRE_TOKEN: true\n";
    }

    oss << "\n========================================\n\n";

    return oss.str();
}

// Function to print dry-run configuration to stdout
inline void printDryRunConfig(const ServerConfig& config) {
    std::cout << generateDryRunConfig(config);
}

} // namespace Tau5CLI

#endif // TAU5_CLI_ARGS_H