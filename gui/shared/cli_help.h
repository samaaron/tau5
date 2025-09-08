#ifndef CLI_HELP_H
#define CLI_HELP_H

#include <string>
#include "common.h"

namespace Tau5CLI {

/**
 * Generate complete help text for command-line usage
 * @param type Binary type (Gui or Node) to customize help text
 * @param programName Name of the executable (e.g., "tau5" or "tau5-node")
 * @return Formatted help text string ready for console output
 */
std::string generateHelpText(Tau5Common::BinaryType type, const char* programName);

/**
 * Generate version string with optional commit hash
 * @param type Binary type to include in version string
 * @return Version string in format "tau5[-node] version X.Y.Z (commit)"
 */
std::string generateVersionString(Tau5Common::BinaryType type);

} // namespace Tau5CLI

#endif // CLI_HELP_H