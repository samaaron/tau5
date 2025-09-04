#ifndef TAU5_CLI_ARGS_H
#define TAU5_CLI_ARGS_H

#include <cstring>
#include <string>
#include <cstdlib>
#include <QtCore/QtGlobal>
#include <QtCore/QCoreApplication>

namespace Tau5CLI {

struct CommonArgs {
    // Runtime environment (controls MIX_ENV)
    enum class Env {
        Default,  // Not specified, will use prod
        Dev,      // Development environment
        Prod,     // Production environment
        Test      // Test environment
    };
    Env env = Env::Default;
    
    // Deployment mode override (tau5-node only, controls TAU5_MODE)
    enum class Mode {
        Default,  // Not specified, binary sets its own default
        Node,     // Local headless server with NIFs
        Central   // Public coordinator without NIFs
    };
    Mode mode = Mode::Default;
    
    // Port configuration
    quint16 portLocal = 0;    // Local web UI port (0 = random)
    quint16 portPublic = 0;   // Public endpoint port (0 = disabled)
    quint16 portMcp = 0;      // MCP services port (0 = use default 5555 when enabled)
    quint16 portChrome = 0;   // Chrome DevTools port (0 = use default 9223 when enabled)
    
    // Quick setup flags
    bool devtools = false;     // Convenience flag: enables dev mode + MCP + Chrome DevTools + Tidewave
    
    // Optional features (default: disabled, flag enables)
    bool mcp = false;              // Enable MCP endpoint
    bool tidewave = false;         // Tidewave on MCP endpoint (implies mcp)
    bool chromeDevtools = false;   // Enable Chrome DevTools
    bool repl = false;             // Elixir REPL (dev mode only)
    bool verbose = false;          // Verbose logging
    bool debugPane = true;         // Debug pane (tau5 only, default enabled)
    
    // NIF control (default: enabled, --no-* disables)
    bool noMidi = false;           // Disable MIDI support
    bool noLink = false;           // Disable Ableton Link support
    bool noDiscovery = false;      // Disable network discovery
    bool noNifs = false;           // Disable all NIFs
    
    // Other
    bool check = false;            // Verify installation
    bool showHelp = false;         // Show help
    bool showVersion = false;      // Show version
    
    // Path overrides
    std::string serverPath;        // Override server path (instead of using TAU5_SERVER_PATH env var)
    
    // Error handling
    bool hasError = false;
    std::string errorMessage;
};


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
    // Environment selection
    if (std::strcmp(arg, "--env-dev") == 0) {
        if (args.devtools && args.env != CommonArgs::Env::Dev) {
            args.hasError = true;
            args.errorMessage = "--env-dev conflicts with --devtools which already set development environment";
            return true;
        }
        args.env = CommonArgs::Env::Dev;
        return true;
    } else if (std::strcmp(arg, "--env-prod") == 0) {
        if (args.devtools) {
            args.hasError = true;
            args.errorMessage = "--env-prod conflicts with --devtools (which enables development environment)";
            return true;
        }
        args.env = CommonArgs::Env::Prod;
        return true;
    } else if (std::strcmp(arg, "--env-test") == 0) {
        if (args.devtools) {
            args.hasError = true;
            args.errorMessage = "--env-test conflicts with --devtools (which enables development environment)";
            return true;
        }
        args.env = CommonArgs::Env::Test;
        return true;
    }
    // Quick development setup
    else if (std::strcmp(arg, "--devtools") == 0) {
        if (args.env != CommonArgs::Env::Default && args.env != CommonArgs::Env::Dev) {
            args.hasError = true;
            args.errorMessage = "--devtools requires development environment but a different environment was already specified";
            return true;
        }
        args.devtools = true;
        // This implies several other flags
        args.env = CommonArgs::Env::Dev;
        args.mcp = true;
        args.tidewave = true;
        args.chromeDevtools = true;
        return true;
    }
    // Deployment mode override (tau5-node only, but parse here for consistency)
    else if (std::strcmp(arg, "--mode-node") == 0) {
        args.mode = CommonArgs::Mode::Node;
        return true;
    } else if (std::strcmp(arg, "--mode-central") == 0) {
        args.mode = CommonArgs::Mode::Central;
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
        if (!args.hasError) args.mcp = true;  // Specifying a port implies enabling MCP
        return true;
    } else if (std::strcmp(arg, "--port-chrome-dev") == 0) {
        parsePort(nextArg, i, args.portChrome, args, "--port-chrome-dev");
        if (!args.hasError) args.chromeDevtools = true;  // Specifying a port implies enabling Chrome DevTools
        return true;
    }
    // Optional features (enable with flag)
    else if (std::strcmp(arg, "--mcp") == 0) {
        args.mcp = true;
        return true;
    } else if (std::strcmp(arg, "--tidewave") == 0) {
        args.tidewave = true;
        args.mcp = true;  // Tidewave requires MCP
        return true;
    } else if (std::strcmp(arg, "--chrome-devtools") == 0) {
        args.chromeDevtools = true;
        return true;
    } else if (std::strcmp(arg, "--repl") == 0) {
        args.repl = true;
        return true;
    } else if (std::strcmp(arg, "--verbose") == 0) {
        args.verbose = true;
        return true;
    } else if (std::strcmp(arg, "--debug-pane") == 0) {
        args.debugPane = true;
        return true;
    }
    // Disable features (--no-* pattern)
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
    } else if (std::strcmp(arg, "--no-debug-pane") == 0) {
        args.debugPane = false;
        return true;
    }
    // Path configuration
    else if (std::strcmp(arg, "--server-path") == 0) {
        if (nextArg != nullptr) {
            args.serverPath = nextArg;
            i++; // Consume next arg
        } else {
            args.hasError = true;
            args.errorMessage = "--server-path requires a path";
        }
        return true;
    }
    // Other
    else if (std::strcmp(arg, "--check") == 0) {
        args.check = true;
        return true;
    } else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
        args.showHelp = true;
        return true;
    } else if (std::strcmp(arg, "--version") == 0) {
        args.showVersion = true;
        return true;
    }
    
    return false; // Not a shared argument
}

// Validate arguments for conflicts and dependencies
// Returns true if valid, false if there are issues (error message will be set)
inline bool validateArguments(CommonArgs& args) {
    // If --devtools was used, it has already set everything correctly
    if (args.devtools) {
        return true;
    }
    
    // REPL only works in dev environment
    if (args.repl && args.env != CommonArgs::Env::Dev) {
        args.hasError = true;
        args.errorMessage = "--repl only works in development environment (use --env-dev or --devtools)";
        return false;
    }
    
    // Check port conflicts
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
    
    // Validate dependent features
    if (args.tidewave && !args.mcp) {
        // This shouldn't happen as tidewave flag sets mcp, but double-check
        args.mcp = true;
    }
    
    if (args.repl) {
        // REPL only works in dev environment
        bool isDevEnv = (args.env == CommonArgs::Env::Dev);
        if (!isDevEnv && args.env != CommonArgs::Env::Default) {
            // Only error if environment was explicitly set to non-dev
            args.hasError = true;
            args.errorMessage = "--repl requires development environment (--env-dev or --devtools)";
            return false;
        }
        // If environment is Default, we'll set it to dev
        if (args.env == CommonArgs::Env::Default) {
            args.env = CommonArgs::Env::Dev;
        }
    }
    
    // If tidewave is requested, ensure we're in dev environment
    if (args.tidewave) {
        bool isDevEnv = (args.env == CommonArgs::Env::Dev);
        if (!isDevEnv && args.env != CommonArgs::Env::Default) {
            // Tidewave only works in dev environment
            args.hasError = true;
            args.errorMessage = "--tidewave requires development environment (--env-dev or --devtools)";
            return false;
        }
        // Auto-enable dev environment if not explicitly set
        if (args.env == CommonArgs::Env::Default) {
            args.env = CommonArgs::Env::Dev;
        }
    }
    
    // Validate port ranges (0 is allowed as it means "disabled" or "auto-allocate")
    auto validatePort = [&](quint16 port, const char* name) {
        if (port > 65535) {  // This shouldn't happen due to quint16, but be defensive
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
    
    // If MCP port is specified but MCP not enabled, enable it
    if (args.portMcp > 0 && !args.mcp) {
        args.mcp = true;
    }
    
    // If Chrome port is specified but chrome devtools not enabled, enable it
    if (args.portChrome > 0 && !args.chromeDevtools) {
        args.chromeDevtools = true;
    }
    
    // Validate deployment mode combinations
    if (args.mode == CommonArgs::Mode::Central) {
        // Central mode should disable all NIFs
        if (!args.noNifs) {
            // Auto-disable NIFs for central mode
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
    // Set MIX_ENV based on runtime environment
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
            // Don't set, use system default or Phoenix default (prod)
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
    }
    
    // MCP configuration
    if (args.mcp) {
        qputenv("TAU5_MCP_ENABLED", "true");
        // Use specified port or default
        quint16 mcpPort = args.portMcp > 0 ? args.portMcp : 5555;
        qputenv("TAU5_MCP_PORT", std::to_string(mcpPort).c_str());
    }
    
    // Chrome DevTools configuration
    if (args.chromeDevtools) {
        qputenv("TAU5_DEVTOOLS_ENABLED", "true");
        // Use specified port or default
        quint16 chromePort = args.portChrome > 0 ? args.portChrome : 9223;
        qputenv("TAU5_DEVTOOLS_PORT", std::to_string(chromePort).c_str());
    }
    
    // Tidewave configuration (only in dev environment)
    if (args.tidewave) {
        bool isDevEnv = (args.env == CommonArgs::Env::Dev || 
                        qgetenv("MIX_ENV") == "dev");
        if (isDevEnv) {
            qputenv("TAU5_TIDEWAVE_ENABLED", "true");
        }
        // Silently ignore in prod environment
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
    
    // Dev tools
    if (args.repl) {
        // Elixir REPL only works in dev environment for security
        bool isDevEnv = (args.env == CommonArgs::Env::Dev || 
                        qgetenv("MIX_ENV") == "dev");
        if (isDevEnv) {
            qputenv("TAU5_ELIXIR_REPL_ENABLED", "true");
        }
        // Silently ignore in prod mode
    }
    if (args.verbose) {
        qputenv("TAU5_VERBOSE", "true");
    }
}

} // namespace Tau5CLI

#endif // TAU5_CLI_ARGS_H