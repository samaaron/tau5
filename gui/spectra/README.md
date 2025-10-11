# Tau5 Spectra

## Overview
Tau5 Spectra provides programmatic access to Chrome DevTools Protocol functionality through the Model Context Protocol (MCP). This allows AI assistants like Claude to inspect and manipulate the DOM, execute JavaScript, monitor console output, and analyze application logs.

## Architecture
The MCP server consists of three main components:

1. **MCPServerStdio** - A stdio-based JSON-RPC server that implements the MCP protocol
2. **CDPClient** - A WebSocket client that connects to Chrome DevTools Protocol
3. **CDPBridge** - The bridge that connects MCP commands to CDP operations

## How It Works
1. When Tau5 starts in dev mode, it enables Chrome DevTools Protocol (default port 9220 for channel 0, or 922X where X is the channel)
2. The MCP server (tau5-spectra) runs as a separate process communicating via stdio
3. The CDPClient connects to the Chrome DevTools Protocol via WebSocket
4. MCP tools are registered to provide various DevTools and logging functionalities

## Available MCP Tools

### Important Distinction Between Tool Categories

Tau5 Spectra provides two distinct categories of tools:

1. **`chromium_devtools_*` tools** - Interact with the browser/Chrome DevTools Protocol
   - These tools communicate with the running browser instance
   - They inspect and manipulate the DOM, execute JavaScript, monitor console output
   - All operations happen through the Chrome DevTools Protocol WebSocket connection

2. **`tau5_logs_*` tools** - Interact with Tau5 application logs on the filesystem
   - These tools read log files directly from disk
   - They search and analyze Tau5's application logs (beam, gui, mcp)
   - These are NOT browser console logs - they are application-level logs written to files

---

## Chrome DevTools Tools (`chromium_devtools_*`)

These tools interact with the browser through Chrome DevTools Protocol:

### DOM Inspection
- **chromium_devtools_getDocument** - Get the full DOM document structure
- **chromium_devtools_querySelector** - Find elements matching a CSS selector
- **chromium_devtools_getOuterHTML** - Get the outer HTML of a DOM node

### JavaScript Execution
- **chromium_devtools_evaluateJavaScript** - Execute JavaScript in the page context

### DOM Manipulation
- **chromium_devtools_setAttribute** - Set an attribute on a DOM element
- **chromium_devtools_removeAttribute** - Remove an attribute from a DOM element

### Navigation
- **chromium_devtools_navigate** - Navigate within the Tau5 application
  - **Always use relative URLs** for normal navigation:
    - Home: `/`
    - Specific page: `/my-page`
    - Navigate up: `../`
  - Spectra automatically handles Tau5's dynamic port allocation
  - **Blocked**: `/dev/*` paths (internal debug pages)
  - **NOT for normal use**: External URLs (e.g., `https://example.com`) only work when Tau5 runs with `--local-only=false` and are strictly for testing external sites, not app navigation

### Style Inspection
- **chromium_devtools_getComputedStyle** - Get computed styles for an element

### Browser Console
- **chromium_devtools_getConsoleMessages** - Get console messages from the browser
  - Supports filtering by level, search text, regex patterns, and time ranges
  - Default limit: 100 messages (use `limit: -1` for all)
  - **`since_last_call` parameter**: Defaults to `false`. Set to `true` only for streaming new messages
    - **IMPORTANT**: Automatically ignored when using search/filter parameters (searches always query full history)
    - Only useful for monitoring new messages without any filters
- **chromium_devtools_clearConsoleMessages** - Clear browser console messages

### Network Monitoring
- **chromium_devtools_getNetworkRequests** - Get network request information
- **chromium_devtools_getResponseBody** - Get response body for a network request

### Performance and Memory
- **chromium_devtools_getMemoryUsage** - Get memory usage statistics
- **chromium_devtools_getPerformanceTimeline** - Get performance timeline data
- **chromium_devtools_getJavaScriptProfile** - Get JavaScript profiling data

### Security and Workers
- **chromium_devtools_getSecurityState** - Get security state information
- **chromium_devtools_getWorkers** - Get information about web workers
- **chromium_devtools_getCrossOriginIsolationStatus** - Get cross-origin isolation status

### DOM Monitoring
- **chromium_devtools_startDOMMutationObserver** - Start observing DOM mutations
- **chromium_devtools_stopDOMMutationObserver** - Stop observing DOM mutations
- **chromium_devtools_getDOMMutations** - Get recorded DOM mutations
- **chromium_devtools_clearDOMMutations** - Clear recorded DOM mutations

### WebSockets and Resources
- **chromium_devtools_getWebSocketFrames** - Get WebSocket frame data
- **chromium_devtools_clearWebSocketFrames** - Clear WebSocket frame data
- **chromium_devtools_getLoadedResources** - Get information about loaded resources

### Audio and WebAssembly
- **chromium_devtools_getAudioContexts** - Get audio context information
- **chromium_devtools_getAudioWorkletState** - Get audio worklet state
- **chromium_devtools_monitorWasmInstantiation** - Monitor WebAssembly instantiation

### Advanced DevTools
- **chromium_devtools_getProperties** - Get properties of a JavaScript object
- **chromium_devtools_releaseObject** - Release a remote object
- **chromium_devtools_callMethod** - Call a Chrome DevTools Protocol method directly
- **chromium_devtools_getSelectionInfo** - Get information about text selection
- **chromium_devtools_getExceptions** - Get JavaScript exceptions

---

## Tau5 Application Log Tools (`tau5_logs_*`)

These tools read and analyze Tau5's application logs directly from the filesystem:

### Log Search and Analysis
- **tau5_logs_search** - Search Tau5 application logs with regex patterns and filters
  - Search across multiple log sessions
  - Support for plain text and regex patterns
  - Filter by log levels and context
  - Returns results in JSON or text format
  - **NOTE**: These are filesystem logs, NOT browser console logs

- **tau5_logs_get** - Read raw Tau5 application logs from filesystem
  - Access beam, gui, and mcp logs
  - Read specified number of lines from log files
  - **NOTE**: These are filesystem logs, NOT browser console logs

- **tau5_logs_getSessions** - List all available Tau5 log sessions with metadata
  - Get information about all log sessions on disk
  - Returns session timestamps and metadata
  - **NOTE**: These are filesystem log sessions, NOT browser sessions

---

## Usage

### Running the MCP Server
Tau5 Spectra runs as a standalone executable that bridges AI assistants to Tau5's Chrome DevTools and application logs. It must be configured in your AI assistant's MCP settings.

### Configuration for Claude Desktop
Add to your Claude Desktop configuration:

Windows:
```json
{
  "mcpServers": {
    "tau5-spectra": {
      "command": "C:\\path\\to\\tau5\\bin\\win\\dev-tau5-spectra.bat",
      "args": ["--channel", "0"]
    }
  }
}
```

macOS:
```json
{
  "mcpServers": {
    "tau5-spectra": {
      "command": "/path/to/tau5/bin/mac/dev-tau5-spectra.sh",
      "args": ["--channel", "0"]
    }
  }
}
```

Linux:
```json
{
  "mcpServers": {
    "tau5-spectra": {
      "command": "/path/to/tau5/bin/linux/dev-tau5-spectra.sh",
      "args": ["--channel", "0"]
    }
  }
}
```

The server communicates via stdio (stdin/stdout) using the MCP protocol.

### Channel Configuration
Tau5 supports channels 0-9 to allow multiple instances to run simultaneously. Each channel uses different default ports:
- Channel 0 (default): Chrome DevTools port 9220
- Channel 1: Chrome DevTools port 9221
- Channel 2: Chrome DevTools port 9222
- And so on...

To specify a different channel (e.g., channel 3):
```json
{
  "mcpServers": {
    "tau5-spectra": {
      "command": "/path/to/tau5/bin/linux/dev-tau5-spectra.sh",
      "args": ["--channel", "3"]
    }
  }
}
```

### Custom DevTools Port
You can also explicitly override the port (this takes precedence over channel):
```json
{
  "mcpServers": {
    "tau5-spectra": {
      "command": "/path/to/tau5/bin/linux/dev-tau5-spectra.sh",
      "args": ["--port-chrome-dev", "9999"]
    }
  }
}
```

### Debug Mode
To enable debug logging, add the `--debug` flag:
```json
{
  "mcpServers": {
    "tau5-spectra": {
      "command": "/path/to/tau5/bin/linux/dev-tau5-spectra.sh",
      "args": ["--channel", "0", "--debug"]
    }
  }
}
```
This will create a `tau5-spectra-debug.log` file in the working directory.

## Security
- The MCP server uses stdio transport (no network sockets)
- Chrome DevTools Protocol only listens on localhost (default port 9220 for channel 0)
- CDP is only enabled in dev mode (not in production builds)
- All external navigation is blocked by Tau5's sandboxed web views

## Development Notes
- The CDP connection happens after a 1-second delay to ensure DevTools is ready
- All CDP commands have a 5-second timeout
- The server validates all JSON-RPC requests and has a 64KB message size limit
- Node IDs from querySelector must be used for element-specific operations
- Log files are stored in the Tau5 data directory under `logs/gui/`
- The Spectra MCP logs are stored separately under `logs/mcp/spectra-{port}/`