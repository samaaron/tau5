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
# :desktop - Running as Qt GUI desktop app
# :central - Running as the authoritative tau5.sonic-pi.net server
# :headless - Running as standalone server (default)
config :tau5,
  deployment_mode: System.get_env("TAU5_MODE", "headless") |> String.to_atom()

config :tau5, Tau5.ConfigRepo,
  # Tau5.ConfigRepo.init/2 will inject :database at boot-time
  pool_size: 1,
  busy_timeout: 5_000,
  journal_mode: :wal

# Security configuration for development tools
# IMPORTANT: These should NEVER be enabled in production!
if config_env() == :prod do
  # Disable all development routes in production
  config :tau5, dev_routes: false
  
  # Explicitly disable console in production
  config :tau5, console_enabled: false
end

if config_env() == :prod do
  # The secret key base is used to sign/encrypt cookies and other secrets.
  # A default value is used in config/dev.exs and config/test.exs but you
  # want to use a different value for prod and you most likely don't want
  # to check this value into version control, so we use an environment
  # variable instead.
  secret_key_base =
    System.get_env("SECRET_KEY_BASE") ||
      raise """
      environment variable SECRET_KEY_BASE is missing.
      You can generate one by calling: mix phx.gen.secret
      """

  host = System.get_env("PHX_HOST") || "127.0.0.1"
  port = String.to_integer(System.get_env("PORT") || "4000")

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
