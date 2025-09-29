#!/bin/bash
# WSL Bridge for Tau5 Spectra MCP Server
# Maintains persistent stdio connection to Windows executable

# Using the .exe from Claude Code results in the first call working and then subsequent calls failing.
# This is likely due to how WSL handles process management and stdio streams.
# This approach appears to be more stable though.

# From Claude Code running inside WSL on Windows:
# claude mcp add spectra bin/win/wsl-spectra-bridge.sh



exec /mnt/c/Users/samaa/Development/tau5/gui/build/bin/tau5-spectra.exe "$@"