import Config

# config/runtime.exs is executed for all environments, including
# during releases. It is executed after compilation and before the
# system starts, so it is typically used to load production configuration
# and secrets from environment variables or elsewhere. Do not define
# any compile-time configuration in here, as it won't be applied.
# The block below contains prod specific runtime configuration.

# ## Using releases
#
# If you use `mix release`, you need to explicitly enable the server
# by passing the PHX_SERVER=true when you start it:
#
#     PHX_SERVER=true bin/tau5 start
#
# Alternatively, you can use `mix phx.gen.release` to generate a `bin/server`
# script that automatically sets the env var above.
if System.get_env("PHX_SERVER") do
  config :tau5, Tau5Web.Endpoint, server: true
end

# Configure heartbeat monitoring
# Set TAU5_HEARTBEAT_ENABLED=true to enable heartbeat monitoring
# (required when running from GUI to prevent zombie processes)
config :tau5, :heartbeat_enabled,
  System.get_env("TAU5_HEARTBEAT_ENABLED", "false") == "true"

# Deployment mode configuration
# TAU5_MODE is set by the binaries:
# - tau5 binary sets "gui" for desktop app
# - tau5-node binary sets "node" for headless server (or "central" with --target-central flag)
# - Direct mix phx.server uses env var or defaults to "node"
config :tau5,
  deployment_mode: System.get_env("TAU5_MODE", "node") |> String.to_atom()

config :tau5, Tau5.ConfigRepo,
  # Tau5.ConfigRepo.init/2 will inject :database at boot-time
  pool_size: 1,
  busy_timeout: 5_000,
  journal_mode: :wal

# Configure MCP endpoint and servers
config :tau5, :mcp_enabled,
  System.get_env("TAU5_MCP_ENABLED", "false") == "true"

# Safe port parsing helper
parse_port = fn env_var, default ->
  case System.get_env(env_var, default) do
    "" -> String.to_integer(default)
    value ->
      case Integer.parse(value) do
        {port, ""} when port >= 0 and port <= 65535 -> port
        _ -> 
          IO.warn("Invalid port value for #{env_var}: #{value}, using default: #{default}")
          String.to_integer(default)
      end
  end
end

config :tau5, :mcp_port, parse_port.("TAU5_MCP_PORT", "5555")

config :tau5, :tidewave_enabled,
  System.get_env("TAU5_TIDEWAVE_ENABLED", "false") == "true"

# Security configuration for development tools
# IMPORTANT: These should NEVER be enabled in production!
if config_env() == :prod do
  # Disable all development routes in production
  config :tau5, dev_routes: false
  
  # Explicitly disable console in production
  config :tau5, console_enabled: false
else
  # In dev/test, check if console should be enabled via env var
  # Default to false for security unless explicitly enabled
  # Accept "1", "true", or "yes" as enabled
  repl_enabled = System.get_env("TAU5_ELIXIR_REPL_ENABLED", "false")
  config :tau5, console_enabled: repl_enabled in ["1", "true", "yes"]
end

# Configure NIF services
# These can be disabled via environment variables or based on deployment mode
if config_env() != :test do
  target = System.get_env("TAU5_MODE", "node")
  
  # Central deployment always disables NIFs
  if target == "central" do
    config :tau5, :midi_enabled, false
    config :tau5, :link_enabled, false
    config :tau5, :discovery_enabled, false
  else
    # For gui and node targets, check individual env vars
    config :tau5, :midi_enabled,
      System.get_env("TAU5_MIDI_ENABLED", "true") != "false"

    config :tau5, :link_enabled,
      System.get_env("TAU5_LINK_ENABLED", "true") != "false"

    config :tau5, :discovery_enabled,
      System.get_env("TAU5_DISCOVERY_ENABLED", "true") != "false"
  end
end

# Configure endpoints
# Local port defaults to PORT env var or 0 for random allocation
local_port_default = System.get_env("PORT", "0")
config :tau5, :local_port,
  parse_port.("TAU5_LOCAL_PORT", local_port_default)

# Public endpoint configuration
# Public endpoint is enabled if TAU5_PUBLIC_PORT is set and > 0
public_port = parse_port.("TAU5_PUBLIC_PORT", "0")
config :tau5, :public_port, public_port
config :tau5, :public_endpoint_enabled, public_port > 0

# MCP endpoint is enabled when TAU5_MCP_ENABLED is true
# Port is already configured above


# Configure public endpoint secret key for non-test environments
if config_env() != :test do
  # Get the appropriate secret key base
  main_secret = case config_env() do
    :prod ->
      System.get_env("SECRET_KEY_BASE") || 
        raise "SECRET_KEY_BASE is required in production"
    _ ->
      # In dev, use the configured secret from dev.exs
      Application.get_env(:tau5, Tau5Web.Endpoint)[:secret_key_base]
  end
  
  if main_secret && main_secret != "placeholder_will_be_replaced_by_stdin_config" do
    public_endpoint_secret = :crypto.hash(:sha256, main_secret <> "_public_endpoint")
      |> Base.encode64()
    
    config :tau5, Tau5Web.PublicEndpoint,
      secret_key_base: public_endpoint_secret
  end
end

if config_env() == :prod do
  # The secret key base is used to sign/encrypt cookies and other secrets.
  # A default value is used in config/dev.exs and config/test.exs but you
  # want to use a different value for prod and you most likely don't want
  # to check this value into version control, so we use an environment
  # variable instead.
  # When using stdin config, the secret will be set later in Application.start
  secret_key_base =
    if System.get_env("TAU5_USE_STDIN_CONFIG") == "true" do
      # Use a placeholder that will be replaced when SecureConfig reads stdin
      "placeholder_will_be_replaced_by_stdin_config"
    else
      System.get_env("SECRET_KEY_BASE") ||
        raise """
        environment variable SECRET_KEY_BASE is missing.
        You can generate one by calling: mix phx.gen.secret
        """
    end

  host = System.get_env("PHX_HOST") || "127.0.0.1"
  # Port must be provided by GUI or use 0 for random allocation
  port = parse_port.("PORT", "0")

  config :tau5, :dns_cluster_query, System.get_env("DNS_CLUSTER_QUERY")

  config :tau5, Tau5Web.Endpoint,
    http: [
      ip: {127, 0, 0, 1},
      port: port
    ],
    check_origin: [
      "http://#{host}:#{port}",
      "http://127.0.0.1:#{port}",
      "http://localhost:#{port}"
    ],
    server: true,
    cache_static_manifest: "priv/static/cache_manifest.json",
    secret_key_base: secret_key_base
end
