# Claude.md — Tau5 Development Guidelines

## Project Overview
- Tau5 is a collaborative creative coding platform for music, visuals, and live interactive art.
- Built on the **BEAM VM** with Elixir/Phoenix for the server and **Qt/C++** hosting a full Chromium browser for the GUI.

## Tau5 has two build modes
1. Dev - Development mode - contains built-in dev tools and runs Phoenix in dev mode from source with auto-compile and asset-building enabled by deafult.
2. Release - No dev tools, Phoenix running in production mode from a mix release.

## Deployment Modes (TAU5_MODE)
Tau5 has three deployment modes that control routing and features:
1. **`:gui`** - Desktop app mode. The full app - Qt Chromium Browser + Elixir Phoenix Server + NIFs. Routes `/` to `/app` (MainLive LiveView)
2. **`:node`** - Headless server mode (default). Qt CLI + Elixir Phoenix Server + NIFs. Routes `/` to `/app` (MainLive LiveView)
3. **`:central`** - Live web mode. Elixir Phoenix Server only. Routes `/` to a special WebGL shader landing page (CentralController). Used for central/hosted deployments (e.g., tau5.live)

## Environment
- **Default assumption**: development mode (`MIX_ENV=dev`).
- **Release mode** is explicit and uses `bin/*/build-release*` scripts.

## Build & Run (bin scripts)
- Scripts are the **canonical way** to build/run Tau5. There are script dirs for Windows (win), macOS (mac) and Linux (linux).
- Examples:
  - `bin/linux/dev-tau5-spectra.sh --devtools` — run GUI + Spectra MCP integration.
  - `bin/mac/dev-tau5-gui.sh` — run standalone GUI.
  - `bin\win\dev-tau5-node.bat` — run headless server node.
  - `bin/linux/dev-build-server.sh` — build the Phoenix server only.
  - `bin/mac/build-release.sh` — build production desktop release.

## Server Development
- Phoenix LiveView app is in `server/`:
  - `lib/tau5/` — business logic.
  - `lib/tau5_web/` — web UI + LiveView.
  - `lib/tau5/mcp/` — MCP/AI integration.
- Asset pipeline:
  - **esbuild** bundles JavaScript (see `config/config.exs`).
  - **Tailwind CSS** compiles styles (see `assets/tailwind.config.js`).
  - All dependencies are **vendored**; **no npm** in production.
  - Tailwind config must include LiveView templates in `content` and safelist `phx-*` classes.

## GUI Development
- GUI is a Qt/C++ desktop app (`gui/`).
- Uses Chromium-based rendering to display Phoenix frontend.
- Considered stable infrastructure; changes here are less frequent but may be required.
- Claude should reason about C++/Qt code when work explicitly involves the GUI.

## Rules & Constraints
- ✅ Use provided **bin scripts** for running, building, and testing.
- ✅ Assume we're building and working with dev mode unless explicitly told otherwise.
- ✅ Keep builds **self-contained** — use vendored JS/CSS, esbuild, Tailwind.
- ✅ Safelist `phx-*` classes in Tailwind to preserve LiveView behavior.
- ❌ Do not introduce **npm**, yarn, webpack, or Vite.
- ❌ Do not fetch remote dependencies at build time.
- ❌ Do not suggest restarting the Phoenix server automatically — that is a user action.

## Development Workflow

A common workflow leverages the MCP servers for rapid experimentation:

1. Use **Tidewave** (Elixir MCP) to experiment with server-side changes and observe dynamic behavior
2. Use **Spectra** (Chrome DevTools MCP) to inspect DOM changes and verify UI updates
3. Solidify changes by editing files, which triggers Phoenix auto-reload
4. Verify the reloaded changes using Spectra tools

The Phoenix server auto-reloads on file save:
- Elixir files trigger recompilation
- Assets (CSS/JS) trigger esbuild/Tailwind rebuilds
- LiveView automatically pushes updates to the browser

## MCP / AI Integration

Tau5 provides three MCP servers for development:

1. **Spectra** - Bridge server providing Chrome DevTools access, Tidewave integration, and log viewing
   - Built with `dev-build-spectra.sh` or as part of the GUI build
   - Connects to Chrome DevTools Protocol (port 9220-9225 depending on channel)
   - Access to DOM manipulation, JavaScript execution, and browser inspection

2. **Tidewave** - Elixir runtime introspection and manipulation
   - Available at `http://localhost:555X/tidewave` where X is the channel (0-5)
   - Module introspection, code evaluation, database access
   - Live server state inspection

3. **Tau5 MCP** - End-user functionality (Lua interpreter)
   - Sandboxed code execution environment
   - Available when running with MCP enabled

## MCP Development Techniques

### State Inspection Pattern
Inspect LiveView process state and verify UI synchronization:

```elixir
# Tidewave: Get LiveView state
:sys.get_state({:via, Registry, {Tau5.PubSub, "phx-SESSION_ID"}})
```

```javascript
// Spectra: Verify DOM reflects state
document.querySelector('[data-phx-component]')
```

### Error Debugging Flow
Check errors across server and client:
- Use Tidewave's `get_logs` to check server logs
- Use Spectra's `getConsoleMessages` for browser console
- Cross-reference to identify root causes

### LiveView Hook Debugging
```javascript
// Check registered hooks
window.liveSocket.hooks;

// Verify hook is attached
document.getElementById("element-id").__view;

// Check LiveSocket connection
window.liveSocket.isConnected();

// Enable debug logging
window.liveSocket.enableDebug();
```

### Asset Pipeline Debugging
When CSS/JS changes don't appear:

```javascript
// Check loaded stylesheets
Array.from(document.querySelectorAll('link[rel="stylesheet"]')).map(l => l.href);

// Check loaded scripts
Array.from(document.querySelectorAll('script[src]')).map(s => s.src);

// Verify asset fingerprinting
document.querySelector('[phx-track-static]');
```

## DOM Manipulation Best Practices

⚠️ **CRITICAL: Phoenix LiveView manages the DOM and may re-render at any time**

### Approach (in order of preference)

1. ✅ **FIRST**: Direct text node update
   ```javascript
   document.querySelector('selector').textContent = 'new value';
   // For preserving whitespace:
   document.querySelector('selector').firstChild.nodeValue = 'new value';
   ```

2. ✅ **FOR MULTIPLE**: Use querySelectorAll
   ```javascript
   document.querySelectorAll('selector').forEach(elem => {
     if (elem.textContent.includes('target')) {
       elem.textContent = elem.textContent.replace('target', 'replacement');
     }
   });
   ```

3. ⚠️ **AVOID**: Complex selection range manipulation (often fails in LiveView)
4. ❌ **NEVER**: Make multiple tool calls to find the same element

### JavaScript Execution Guidelines

The `evaluateJavaScript` tool expects valid expressions for console execution:

❌ **DON'T** write:
```javascript
if (condition) {
    'Success';  // Syntax error - loose string literal
}
```

✅ **DO** write:
```javascript
// Option 1: Variable assignment
let result = condition ? 'Success' : 'Error';
result;  // Last line is returned

// Option 2: Direct expression
document.querySelector('.element').textContent = 'new value';
'Updated';  // Simple return confirmation
```

## Key Takeaways
1. **Server development is primary** — focus on Elixir/Phoenix + LiveView + asset pipeline.
2. **GUI is secondary** — Qt/C++ is stable but may require changes.
3. **Always use bin scripts for building and launching** — they set up the environment correctly.
4. **Strict vendoring** — no npm, no external fetches, no alternative build tools.
5. **Dev mode is default** — release is explicit.
6. **Use Spectra** - Spectra provides an extensive suite of MCP tools to help Agents colloaborate with the development of Tau5.
