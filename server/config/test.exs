import Config

config :tau5, :midi_enabled, false
config :tau5, :link_enabled, false
config :tau5, :discovery_enabled, false

# We don't run a server during test. If one is required,
# you can enable the server option below.
config :tau5, Tau5Web.Endpoint,
  http: [ip: {127, 0, 0, 1}, port: 4002],
  secret_key_base: "BGzS0ba64RBujAPYlDYQSC3bH68HEYK+xdm1yxYXvN2bonhdncvSi++DsHWVMJlE",
  server: false

config :tau5, Tau5.ConfigRepo,
  database: ":memory:",
  pool_size: 1,
  queue_target: 10,
  queue_interval: 100,
  timeout: 2000

# In test we don't send emails
config :tau5, Tau5.Mailer, adapter: Swoosh.Adapters.Test

# Disable swoosh api client as it is only required for production adapters
config :swoosh, :api_client, false

# Print only errors during test (suppresses warnings)
config :logger, level: :error

# Initialize plugs at runtime for faster test compilation
config :phoenix, :plug_init_mode, :runtime

# Enable helpful, but potentially expensive runtime checks
config :phoenix_live_view,
  enable_expensive_runtime_checks: true

# Enable console for testing
config :tau5, :dev_routes, true
config :tau5, :console_enabled, true
