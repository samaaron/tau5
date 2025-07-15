# Tau5 DevTools MCP Server

## Overview
The Tau5 DevTools MCP Server provides programmatic access to Chrome DevTools Protocol functionality through the Model Context Protocol (MCP). This allows AI assistants like Claude to inspect and manipulate the DOM, execute JavaScript, and monitor console output in the Tau5 application.

## Architecture
The MCP server consists of three main components:

1. **MCPServerStdio** - A stdio-based JSON-RPC server that implements the MCP protocol
2. **CDPClient** - A WebSocket client that connects to Chrome DevTools Protocol
3. **CDPBridge** - The bridge that connects MCP commands to CDP operations

## How It Works
1. When Tau5 starts in dev mode, it enables Chrome DevTools Protocol on port 9223
2. The MCP server (tau5-devtools-mcp) runs as a separate process communicating via stdio
3. The CDPClient connects to the Chrome DevTools Protocol via WebSocket
4. MCP tools are registered to provide various DevTools functionalities

## Available MCP Tools

### DOM Inspection
- **devtools.getDocument** - Get the full DOM document structure
- **devtools.querySelector** - Find elements matching a CSS selector
- **devtools.getOuterHTML** - Get the outer HTML of a DOM node

### JavaScript Execution
- **devtools.evaluateJavaScript** - Execute JavaScript in the page context

### DOM Manipulation
- **devtools.setAttribute** - Set an attribute on a DOM element
- **devtools.removeAttribute** - Remove an attribute from a DOM element

### Navigation
- **devtools.navigate** - Navigate to a URL

### Style Inspection
- **devtools.getComputedStyle** - Get computed styles for an element


## Usage

### Running the MCP Server
The tau5-devtools-mcp server runs as a standalone executable that bridges AI assistants to Tau5's Chrome DevTools. It must be configured in your AI assistant's MCP settings.

### Configuration for Claude Desktop
Add to your Claude Desktop configuration:
```json
{
  "mcpServers": {
    "tau5-devtools": {
      "command": "C:\\path\\to\\tau5\\bin\\win\\tau5-devtools-mcp.bat",
      "args": []
    }
  }
}
```

The server communicates via stdio (stdin/stdout) using the MCP protocol.

## Security
- The MCP server uses stdio transport (no network sockets)
- Chrome DevTools Protocol only listens on localhost:9223
- CDP is only enabled in dev mode (not in production builds)
- All external navigation is blocked by Tau5's sandboxed web views

## Development Notes
- The CDP connection happens after a 1-second delay to ensure DevTools is ready
- All CDP commands have a 5-second timeout
- The server validates all JSON-RPC requests and has a 64KB message size limit
- Node IDs from querySelector must be used for element-specific operations