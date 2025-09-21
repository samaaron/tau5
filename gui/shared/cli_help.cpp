#include "cli_help.h"
#include "common.h"
#include <sstream>

namespace Tau5CLI {

std::string generateHelpText(Tau5Common::BinaryType type, const char* programName) {
    std::ostringstream help;
    
    help << "Usage: " << programName << " [options]\n"
         << "Options:\n"
         << "\n";

    help << "Enable Features:\n"
         << "  --mcp                    Enable MCP endpoint\n"
         << "  --friend-token [token]   Enable friend authentication\n"
         << "                           (generates secure token if not provided)\n"
         << "                           (automatically enables public endpoint)\n"
         << "  --verbose                Enable verbose logging\n"
         << "\n"
         << "Disable Features:\n"
         << "  --no-midi                Disable MIDI support\n"
         << "  --no-link                Disable Ableton Link support\n"
         << "  --no-discovery           Disable network discovery\n"
         << "  --no-nifs                Disable all NIFs (MIDI, Link, and Discovery)\n";

    if (type == Tau5Common::BinaryType::Node) {
        help << "  --no-local-endpoint      Disable local endpoint completely\n";
    }

    help << "\n"
         << "Port Configuration:\n"
         << "  --channel <0-9>          Channel number (0-9, default: 0)\n"
         << "                           Modifies default ports: MCP=555X";

#ifndef TAU5_RELEASE_BUILD
    if (type == Tau5Common::BinaryType::Gui) {
        help << ", CDP=922X";
    }
#endif

    help << "\n"
         << "  --port-local <n>         Local web UI port (default: random)\n"
         << "  --port-public <n>        Public endpoint port (default: disabled)\n"
         << "  --port-heartbeat <n>     Heartbeat UDP port (default: random)\n"
         << "  --port-mcp <n>           MCP services port (overrides channel default)\n";

#ifndef TAU5_RELEASE_BUILD
    help << "\n"
         << "Development Options:\n"
         << "  --devtools               All-in-one dev setup (";

    if (type == Tau5Common::BinaryType::Node) {
        help << "MCP + Tidewave + REPL)\n";
    } else {
        help << "MCP + Chrome DevTools + Tidewave + REPL)\n";
    }

    help << "  --dev-tidewave           Add Tidewave to MCP endpoint (implies --mcp)\n";

    if (type == Tau5Common::BinaryType::Gui) {
        help << "  --dev-chrome-cdp         Enable Chrome DevTools Protocol\n"
             << "  --dev-port-chrome-cdp <n> Chrome DevTools Protocol port (overrides channel default)\n";
    }

    help << "  --dev-repl               Enable Elixir REPL\n";

    if (type == Tau5Common::BinaryType::Gui) {
        help << "  --dev-no-debug-pane      Disable debug pane\n"
             << "  --dev-allow-remote-access Allow loading remote websites/assets\n"
             << "                           WARNING: For debugging only - reduces security\n";
    }

    if (type == Tau5Common::BinaryType::Node) {
        help << "\n"
             << "Deployment Mode Override:\n"
             << "  --mode-node              Local headless server [default]\n"
             << "                           - Local and MCP endpoints available\n"
             << "                           - Full NIFs and local I/O support\n"
             << "  --mode-central           Public coordinator (tau5.live)\n"
             << "                           - Public web endpoints only\n"
             << "                           - No local endpoints or MCP servers\n"
             << "                           - No NIFs or local I/O capabilities\n";
    }

    help << "  --dev-server-path <path> Override server directory path\n"
         << "  --dev-with-release-server Use compiled release server in production mode\n"
         << "                           (default: development server from source)\n";
#endif

    help << "\n"
         << "Other:\n"
         << "  --check                  Verify installation and exit\n"
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