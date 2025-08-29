#ifndef TAU5_CLI_ARGS_H
#define TAU5_CLI_ARGS_H

#include <cstring>
#include <string>
#include <cstdlib>
#include <QtCore/QtGlobal>
#include <QtCore/QtEnvironmentVariables>

namespace Tau5CLI {

struct CommonArgs {
    bool devMode = false;
    bool enableMcp = false;
    bool enableRepl = false;
    bool verboseLogging = false;
    bool disableMidi = false;
    bool disableLink = false;
    bool disableDiscovery = false;
    quint16 customPort = 0;
    bool showHelp = false;
    bool hasError = false;
    std::string errorMessage;
};

// Parse shared command-line arguments used by both tau5 and tau5-node
// Returns true if the argument was recognized (whether successful or not)
// Check args.hasError to see if there was an error processing the argument
inline bool parseSharedArg(const char* arg, const char* nextArg, int& i, CommonArgs& args) {
    if (std::strcmp(arg, "dev") == 0) {
        args.devMode = true;
        return true;
    } else if (std::strcmp(arg, "--enable-mcp") == 0) {
        args.enableMcp = true;
        return true;
    } else if (std::strcmp(arg, "--enable-repl") == 0) {
        args.enableRepl = true;
        return true;
    } else if (std::strcmp(arg, "--verbose") == 0) {
        args.verboseLogging = true;
        return true;
    } else if (std::strcmp(arg, "--port") == 0) {
        if (nextArg != nullptr) {
            int port = std::atoi(nextArg);
            if (port < 1 || port > 65535) {
                args.hasError = true;
                args.errorMessage = "Port must be between 1 and 65535";
                return true; // Recognized but error
            }
            args.customPort = static_cast<quint16>(port);
            i++; // Consume next arg
        } else {
            args.hasError = true;
            args.errorMessage = "--port requires a number";
            return true; // Recognized but error
        }
        return true;
    } else if (std::strcmp(arg, "--disable-midi") == 0) {
        args.disableMidi = true;
        return true;
    } else if (std::strcmp(arg, "--disable-link") == 0) {
        args.disableLink = true;
        return true;
    } else if (std::strcmp(arg, "--disable-discovery") == 0) {
        args.disableDiscovery = true;
        return true;
    } else if (std::strcmp(arg, "--disable-all") == 0) {
        args.disableMidi = true;
        args.disableLink = true;
        args.disableDiscovery = true;
        return true;
    } else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
        args.showHelp = true;
        return true;
    }
    
    return false; // Not a shared argument
}

inline void applyServiceDisables(const CommonArgs& args) {
    if (args.disableMidi) {
        qputenv("TAU5_MIDI_ENABLED", "false");
    }
    if (args.disableLink) {
        qputenv("TAU5_LINK_ENABLED", "false");
    }
    if (args.disableDiscovery) {
        qputenv("TAU5_DISCOVERY_ENABLED", "false");
    }
}

} // namespace Tau5CLI

#endif // TAU5_CLI_ARGS_H