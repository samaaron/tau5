# This file is responsible for configuring your application
# and its dependencies with the aid of the Config module.
#
# This configuration file is loaded before any dependency and
# is restricted to this project.

# General application configuration
import Config

config :tau5,
  generators: [timestamp_type: :utc_datetime]

# Configures the endpoint
config :tau5, Tau5Web.Endpoint,
  url: [host: "localhost"],
  adapter: Bandit.PhoenixAdapter,
  render_errors: [
    formats: [html: Tau5Web.ErrorHTML, json: Tau5Web.ErrorJSON],
    layout: false
  ],
  pubsub_server: Tau5.PubSub,
  live_view: [signing_salt: "knYdl8z1"]

# Configures the mailer
#
# By default it uses the "Local" adapter which stores the emails
# locally. You can see the emails in your browser, at "/dev/mailbox".
#
# For production it's recommended to configure a different adapter
# at the `config/runtime.exs`.
config :tau5, Tau5.Mailer, adapter: Swoosh.Adapters.Local

# Configure esbuild (the version is required)
# Note - we're putting the esbuild assets in a separate directory
# to the tailwind assets to avoid conflicts.
config :esbuild,
  version: "0.25.4",
  default: [
    args: ~w(
      js/app.js
      --bundle
      --target=es2021
      --outdir=../priv/static/assets
      --public-path=/assets
      --loader:.css=css
      --loader:.woff2=file
      --loader:.woff=file
      --loader:.ttf=file
    ),
    cd: Path.expand("../assets", __DIR__),
    env: %{"NODE_PATH" => Path.expand("../deps", __DIR__)}
  ],
  monaco_worker: [
    args: ~w(
      vendor/monaco-editor/esm/vs/editor/editor.worker.js
      --bundle
      --target=es2021
      --outdir=../priv/static/assets/js/monaco-worker
      ),
    cd: Path.expand("../assets", __DIR__)
  ]

# Configure tailwind (the version is required)
config :tailwind,
  version: "3.4.13",
  default: [
    args: ~w(
      --config=tailwind.config.js
      --input=css/app.css
      --output=../priv/static/assets/tailwind.css
    ),
    cd: Path.expand("../assets", __DIR__)
  ]

# Configures Elixir's Logger
config :logger, :console,
  format: "$time $metadata[$level] $message\n",
  metadata: [:request_id]

# Use Jason for JSON parsing in Phoenix
config :phoenix, :json_library, Jason

# Configure MIME types for SSE
config :mime, :types, %{
  "text/event-stream" => ["event-stream"]
}

# Import environment specific config. This must remain at the bottom
# of this file so it overrides the configuration defined above.
import_config "#{config_env()}.exs"
