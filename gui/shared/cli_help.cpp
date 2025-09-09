#include "cli_help.h"
#include "common.h"
#include <sstream>

namespace Tau5CLI {

std::string generateHelpText(Tau5Common::BinaryType type, const char* programName) {
    std::ostringstream help;
    
    help << "Usage: " << programName << " [options]\n"
         << "Options:\n"
         << "\n";

#ifndef TAU5_RELEASE_BUILD
    help << "Quick Setup:\n"
         << "  --devtools               All-in-one dev setup (";
    
    if (type == Tau5Common::BinaryType::Node) {
        help << "MCP + Tidewave + REPL)\n";
    } else {
        help << "MCP + Chrome DevTools + Tidewave + REPL)\n";
    }
    help << "\n";
    
    // Node-specific deployment mode override
    if (type == Tau5Common::BinaryType::Node) {
        help << "Deployment Mode Override:\n"
             << "  --mode-node              Local headless server [default]\n"
             << "                           - Local and MCP endpoints available\n"
             << "                           - Full NIFs and local I/O support\n"
             << "  --mode-central           Public coordinator (tau5.live)\n"
             << "                           - Public web endpoints only\n"
             << "                           - No local endpoints or MCP servers\n"
             << "                           - No NIFs or local I/O capabilities\n"
             << "\n";
    }
#endif

    help << "Server Configuration:\n"
         << "  --with-release-server    Use compiled release server in production mode\n"
         << "                           (default: development server from source)\n"
         << "\n"
         << "Port Configuration:\n"
         << "  --port-local <n>         Local web UI port (default: random)\n"
         << "  --port-public <n>        Public endpoint port (default: disabled)\n"
         << "  --port-heartbeat <n>     Heartbeat UDP port (default: random)\n";
    
    if (type == Tau5Common::BinaryType::Node) {
        help << "  --no-local-endpoint      Disable local endpoint completely\n";
    }
    
    help << "  --port-mcp <n>           MCP services port (default: 5555 when enabled)\n"
         << "  --friend-token [token]   Enable friend authentication\n"
         << "                           (generates secure token if not provided)\n"
         << "                           (automatically enables public endpoint)\n";

#ifndef TAU5_RELEASE_BUILD
    if (type == Tau5Common::BinaryType::Gui) {
        help << "  --port-chrome-dev <n>    Chrome DevTools port (default: 9223 when enabled)\n";
    }
#endif

    help << "\n"
         << "Optional Features:\n"
         << "  --mcp                    Enable MCP endpoint\n";

#ifndef TAU5_RELEASE_BUILD
    help << "  --tidewave               Add Tidewave to MCP endpoint (implies --mcp)\n";
    if (type == Tau5Common::BinaryType::Gui) {
        help << "  --chrome-devtools        Enable Chrome DevTools\n";
    }
    help << "  --repl                   Enable Elixir REPL\n";
#endif

    help << "  --verbose                Enable verbose logging\n"
         << "\n"
         << "Disable Features:\n"
         << "  --no-midi                Disable MIDI support\n"
         << "  --no-link                Disable Ableton Link support\n"
         << "  --no-discovery           Disable network discovery\n"
         << "  --no-nifs                Disable all NIFs (MIDI, Link, and Discovery)\n";

#ifndef TAU5_RELEASE_BUILD
    if (type == Tau5Common::BinaryType::Gui) {
        help << "  --no-debug-pane          Disable debug pane\n";
    }
#endif

    help << "\n"
         << "Other:\n";

#ifndef TAU5_RELEASE_BUILD
    help << "  --server-path <path>     Override server directory path\n";
#endif

    help << "  --check                  Verify installation and exit\n"
         << "  --help, -h               Show this help message\n"
         << "  --version                Show version information\n"
         << "\n";

    // Binary-specific descriptions
    if (type == Tau5Common::BinaryType::Gui) {
        help << "Tau5 - Desktop application for collaborative live-coding\n"
             << "Creates music and visuals through code. Includes a full GUI interface.\n"
             << "\n"
             << "Note: TAU5_MODE is automatically set to 'gui' for the desktop application.\n";
    } else {
        help << "Tau5 Node - Headless server mode for Tau5\n"
             << "Run Tau5 without a GUI, perfect for servers and remote deployments.\n"
             << "\n"
             << "Note: TAU5_MODE is set to 'node' by default, or 'central' with --mode-central.\n";
    }

    return help.str();
}

std::string generateVersionString(Tau5Common::BinaryType type) {
    std::ostringstream version;
    
    if (type == Tau5Common::BinaryType::Gui) {
        version << "tau5";
    } else {
        version << "tau5-node";
    }
    
    version << " version " << Tau5Common::Config::APP_VERSION;
    
    if (std::string(Tau5Common::Config::APP_COMMIT) != "unknown") {
        version << " (" << Tau5Common::Config::APP_COMMIT << ")";
    }
    
    return version.str();
}

} // namespace Tau5CLI