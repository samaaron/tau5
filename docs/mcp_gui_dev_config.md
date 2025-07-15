# Tau5 GUI Dev MCP Configuration

## Overview
The Tau5 GUI Dev MCP server provides Model Context Protocol access to Chrome DevTools, allowing AI assistants to inspect and manipulate the DOM, execute JavaScript, and monitor the Tau5 application.

## Prerequisites
1. Tau5 must be running in dev mode (`tau5 dev`)
2. Chrome DevTools Protocol is enabled on port 9223 (default)
3. The `tau5-gui-dev-mcp` executable must be built

## Building the MCP Server
```bash
# Windows
cd tau5
cmd.exe /c "bin\win\build-gui.bat"

# macOS/Linux
cd tau5
./bin/mac/build-gui.sh  # or ./bin/linux/build-gui.sh
```

This will build both Tau5 and the tau5-gui-dev-mcp executable.

## Claude Code Configuration

### Quick Setup with CLI (Recommended)

The easiest way to add the MCP server is using the Claude Code CLI:

#### Windows
```bash
claude mcp add tau5-gui-dev cmd.exe /c "C:\path\to\tau5\bin\win\tau5-gui-dev-mcp.bat"
```

#### macOS
```bash
claude mcp add tau5-gui-dev /path/to/tau5/bin/mac/tau5-gui-dev-mcp.sh
```

#### Linux
```bash
claude mcp add tau5-gui-dev /path/to/tau5/bin/linux/tau5-gui-dev-mcp.sh
```

### Manual Configuration (Alternative)

You can also manually edit your Claude Code MCP settings:

#### Windows
```json
{
  "mcpServers": {
    "tau5-gui-dev": {
      "command": "C:\\path\\to\\tau5\\bin\\win\\tau5-gui-dev-mcp.bat",
      "args": []
    }
  }
}
```

#### macOS
```json
{
  "mcpServers": {
    "tau5-gui-dev": {
      "command": "/path/to/tau5/bin/mac/tau5-gui-dev-mcp.sh",
      "args": []
    }
  }
}
```

#### Linux
```json
{
  "mcpServers": {
    "tau5-gui-dev": {
      "command": "/path/to/tau5/bin/linux/tau5-gui-dev-mcp.sh",
      "args": []
    }
  }
}
```

## Custom DevTools Port
If Tau5 is configured to use a different DevTools port, specify it in the args:

```json
{
  "mcpServers": {
    "tau5-gui-dev": {
      "command": "path/to/tau5-gui-dev-mcp",
      "args": ["--devtools-port", "9999"]
    }
  }
}
```

## Available Tools

### DOM Inspection
- `chromium_devtools_getDocument` - Get the full DOM tree
- `chromium_devtools_querySelector` - Find elements by CSS selector
- `chromium_devtools_getOuterHTML` - Get HTML of a specific node

### JavaScript Execution
- `chromium_devtools_evaluateJavaScript` - Execute JavaScript in the page context

### DOM Manipulation
- `chromium_devtools_setAttribute` - Set attributes on elements
- `chromium_devtools_removeAttribute` - Remove attributes from elements

### Navigation
- `chromium_devtools_navigate` - Navigate to a different URL

### Style Inspection
- `chromium_devtools_getComputedStyle` - Get all computed CSS styles for an element

## Usage Workflow

1. Start Tau5 in dev mode:
   ```
   tau5 dev
   ```

2. Claude Code will automatically launch the MCP server when needed

3. Use the tools to inspect and manipulate the application:
   ```
   // Find an element
   chromium_devtools_querySelector({ selector: "#my-button" })
   
   // Execute JavaScript
   chromium_devtools_evaluateJavaScript({ expression: "document.title" })
   
   // Get computed styles
   chromium_devtools_getComputedStyle({ selector: ".my-class" })
   ```

## Troubleshooting

### MCP server fails to connect
- Ensure Tau5 is running in dev mode
- Check that port 9223 is not blocked by firewall
- Look for "Chrome DevTools Protocol enabled on port 9223" in Tau5 logs

### Tools return "Not connected" errors
- The MCP server needs a moment to connect after starting
- Check stderr output for connection status messages

### Executable not found
- Ensure you've built the project with the GUI build scripts
- Check that the launcher script path is correct in your MCP configuration

## Complete Configuration Example

Here's a complete example with both Tidewave (backend) and DevTools (frontend) MCP servers:

```json
{
  "mcpServers": {
    "tidewave": {
      "url": "http://localhost:5555/tidewave",
      "transport": "sse"
    },
    "tau5-gui-dev": {
      "command": "C:\\tau5\\bin\\win\\tau5-gui-dev-mcp.bat",
      "args": []
    }
  }
}
```

This gives you complete access to both the Elixir backend (via Tidewave) and the Chrome frontend (via DevTools MCP).