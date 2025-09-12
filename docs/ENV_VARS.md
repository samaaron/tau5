# Tau5 Environment Variables

This document describes all environment variables used by Tau5 across its different components.

## Core Environment Variables

### Runtime Environment

| Variable | Values | Default | Description |
|----------|--------|---------|-------------|
| `MIX_ENV` | `dev`, `prod`, `test` | `prod` | Elixir/Phoenix runtime environment. Controls compilation, hot-reload, and optimizations |
| `TAU5_MODE` | `gui`, `node`, `central` | Varies | Deployment target. Set automatically by binaries (`tau5` sets `gui`, `tau5-node` sets `node`) |
| `PHX_SERVER` | `true`, `false` | `false` | Enables Phoenix server (required for releases) |
| `PHX_HOST` | hostname/IP | `127.0.0.1` | Phoenix server host binding |

### Port Configuration

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `PORT` | 0-65535 | `0` (random) | Primary local web UI port |
| `TAU5_LOCAL_PORT` | 0-65535 | Uses `PORT` | Override for local web UI port |
| `TAU5_PUBLIC_PORT` | 0-65535 | `0` (disabled) | Public endpoint port (0 = disabled) |
| `TAU5_MCP_PORT` | 0-65535 | `5555` when enabled | MCP services endpoint port (0 = disabled) |
| `TAU5_DEVTOOLS_PORT` | 0-65535 | `9223` | Chrome DevTools debugging port |

### Endpoint Control

| Variable | Values | Default | Description |
|----------|--------|---------|-------------|
| `TAU5_NO_LOCAL_ENDPOINT` | `true`, `false` | `false` | Disable local endpoint entirely (for headless deployments) |

### Feature Flags

#### Development Tools

| Variable | Values | Default | Description |
|----------|--------|---------|-------------|
| `TAU5_MCP_ENABLED` | `true`, `false` | `false` | Enable MCP endpoint |
| `TAU5_ENABLE_DEV_MCP` | `true`, `false` | `false` | Alternative flag to enable MCP in development |
| `TAU5_TIDEWAVE_ENABLED` | `true`, `false` | `false` | Enable Tidewave on MCP endpoint (dev only) |
| `TAU5_TIDEWAVE_LOGGING` | `true`, `false` | `false` | Enable Tidewave request/response logging |
| `TAU5_DEVTOOLS_ENABLED` | `true`, `false` | `false` | Enable Chrome DevTools |
| `TAU5_ELIXIR_REPL_ENABLED` | `true`, `false` | `false` | Enable Elixir REPL console (dev only) |
| `TAU5_VERBOSE` | `true`, `false` | `false` | Enable verbose logging |

#### Local I/O Services (NIFs)

| Variable | Values | Default | Description |
|----------|--------|---------|-------------|
| `TAU5_MIDI_ENABLED` | `true`, `false` | `true`* | Enable MIDI support |
| `TAU5_LINK_ENABLED` | `true`, `false` | `true`* | Enable Ableton Link support |
| `TAU5_DISCOVERY_ENABLED` | `true`, `false` | `true`* | Enable network discovery |

*Note: NIFs are automatically disabled when `TAU5_MODE=central`

### Security & Authentication

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `SECRET_KEY_BASE` | String | Required in prod | Phoenix secret key for signing/encrypting cookies and sessions |
| `TAU5_SESSION_TOKEN` | String | Auto-generated | Security token for console access |
| `TAU5_USE_STDIN_CONFIG` | `true`, `false` | `true` (GUI) | Secure secret passing via stdin (see below) |

#### Friend Mode Authentication

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `TAU5_FRIEND_MODE` | `true`, `false` | `false` | Enable friend mode authentication for public endpoints |
| `TAU5_FRIEND_TOKEN` | String | None | Authentication token for friend mode access |
| `TAU5_FRIEND_REQUIRE_TOKEN` | `true`, `false` | `false` | Require friend token for all public access (reject unauthenticated) |

Friend mode provides a middle tier of access between full local access and restricted public access. Friend-authenticated users get:
- Filesystem access, MIDI support, Ableton Link
- Device pairing capabilities  
- Extended Lua privileges
- No admin tools or console access (for security)

#### Secure Configuration via Stdin (`TAU5_USE_STDIN_CONFIG`)

When `TAU5_USE_STDIN_CONFIG=true`, sensitive secrets are passed to the BEAM process via stdin instead of environment variables, preventing them from being visible in process listings (`ps`), `/proc` files, or environment dumps.

**How it works:**
1. The parent process (GUI or tau5-node) generates secure tokens
2. The BEAM process starts with `TAU5_USE_STDIN_CONFIG=true`
3. Parent writes 4 lines to BEAM's stdin:
   - Line 1: Session token
   - Line 2: Heartbeat token  
   - Line 3: Heartbeat port
   - Line 4: SECRET_KEY_BASE
4. BEAM reads these from stdin and uses them instead of env vars

**Security benefits:**
- Secrets never appear in `ps aux` or `/proc/[pid]/environ`
- Direct pipe between parent/child - inaccessible to other processes
- Secrets only briefly in memory during transfer
- Default enabled for GUI mode for enhanced security

### Process Management

| Variable | Values | Default | Description |
|----------|--------|---------|-------------|
| `TAU5_HEARTBEAT_ENABLED` | `true`, `false` | `false` | Enable heartbeat monitoring (GUI sets this) |
| `TAU5_HEARTBEAT_PORT` | Port | Random | Heartbeat server port |
| `TAU5_HEARTBEAT_TOKEN` | String | Random | Heartbeat authentication token |

### Build & Development

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `TAU5_SERVER_PATH` | Path | Auto-detected | Override path to server directory |
| `TAU5_LOG_DIR` | Path | Platform-specific | Override log directory location |
| `QT_INSTALL_LOCATION` | Path | System Qt | Qt installation path (build-time) |
| `TAU5_BUILD_TARGET` | `arm64`, `x86_64` | Auto-detected | Target architecture |

### Qt/GUI Specific

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `QTWEBENGINE_CHROMIUM_FLAGS` | String | Set by app | Chromium engine flags |
| `QT_QPA_PLATFORM` | String | Platform default | Qt platform plugin (e.g., `offscreen` for headless) |
| `DISPLAY` | String | System | X11 display (Linux) |

## Environment Variable Sources

Environment variables can be set in multiple ways:

1. **Command-line arguments** - The tau5 and tau5-node binaries parse arguments and set corresponding environment variables
2. **System environment** - Variables set in the shell or system
3. **Launch scripts** - The bin/[platform]/tau5.sh scripts set defaults
4. **Binary defaults** - Each binary sets its own defaults (e.g., TAU5_MODE)

## Common Configurations

### Development with All Tools

```bash
tau5 --devtools
```

This sets:
- `MIX_ENV=dev`
- `TAU5_MCP_PORT=5555`
- `TAU5_TIDEWAVE_ENABLED=true`
- `TAU5_DEVTOOLS_ENABLED=true`
- `TAU5_DEVTOOLS_PORT=9223`
- `TAU5_ELIXIR_REPL_ENABLED=true`

### Production Desktop App

```bash
tau5 --env-prod
```

This sets:
- `MIX_ENV=prod`
- `TAU5_MODE=gui`
- All NIFs enabled by default

### Headless Server for Local Network

```bash
tau5-node --mcp --port-local 8080
```

This sets:
- `TAU5_MODE=node`
- `TAU5_LOCAL_PORT=8080`
- `TAU5_MCP_PORT=5555`
- All NIFs enabled

### Public Web Server (tau5.live)

```bash
tau5-node --mode-central --port-public 443
```

This sets:
- `TAU5_MODE=central`
- `TAU5_PUBLIC_PORT=443`
- All NIFs disabled
- No local endpoints
- No MCP services

### Headless Raspberry Pi with Friend Authentication

```bash
TAU5_FRIEND_MODE=true TAU5_FRIEND_TOKEN=my-secret-token \
tau5-node --no-local-endpoint --port-public 7001
```

This sets:
- `TAU5_NO_LOCAL_ENDPOINT=true` (no local port)
- `TAU5_PUBLIC_PORT=7001`
- `TAU5_FRIEND_MODE=true`
- `TAU5_FRIEND_TOKEN=my-secret-token`
- Friend-authenticated users get elevated privileges
- Access URL: `http://[pi-ip]:7001/?friend_token=my-secret-token`

## Platform-Specific Notes

### Windows
- Console output uses UTF-8 (`SetConsoleOutputCP(CP_UTF8)`)
- Uses `_build\prod\rel\tau5\bin\tau5.bat` for production

### Linux
- Logs typically in `~/.local/share/Tau5/logs/`
- May need `DISPLAY` set for GUI mode

### macOS
- App bundle at `gui/build/Tau5.app`
- Logs in `~/Library/Application Support/Tau5/logs/`

## Debugging

To see all environment variables being set:

```bash
tau5 --check --verbose
```

This will:
1. Parse arguments and set environment variables
2. Run tests to verify correct parsing
3. Display the final configuration
4. Check for production release (if applicable)

## Security Considerations

- **Never commit `SECRET_KEY_BASE`** - Generate with `mix phx.gen.secret`
- **Development tools are disabled in production** - `TAU5_ELIXIR_REPL_ENABLED` and `TAU5_TIDEWAVE_ENABLED` only work in dev mode
- **Console access requires token** - `TAU5_SESSION_TOKEN` is required for /console access
- **MCP endpoint is local only** - Binds to 127.0.0.1 by default