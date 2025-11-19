defmodule Tau5Web.Endpoint do
  use Phoenix.Endpoint, otp_app: :tau5

  # The session will be stored in the cookie and signed,
  # this means its contents can be read but not tampered with.
  # Set :encryption_salt if you would also like to encrypt it.
  @session_options [
    store: :cookie,
    key: "_tau5_key",
    signing_salt: "Ft4u6yx2",
    same_site: "Lax",
    # Prevent JavaScript access to cookies
    http_only: true
  ]

  socket "/live", Phoenix.LiveView.Socket,
    websocket: [
      connect_info: [
        session: @session_options,
        peer_data: true,
        x_headers: ["x-forwarded-for"],
        uri: true
      ]
    ],
    longpoll: [
      connect_info: [
        session: @session_options,
        peer_data: true,
        x_headers: ["x-forwarded-for"],
        uri: true
      ]
    ]

  # Apply COOP/COEP headers app-wide for SharedArrayBuffer support (required by SuperSonic)
  plug Tau5Web.Plugs.CrossOriginIsolation

  # Serve at "/" the static files from "priv/static" directory.
  #
  # You should set gzip to true if you are running phx.digest
  # when deploying your static files in production.
  # Note: SuperSonic WASM audio engine is vendored at priv/static/supersonic/
  # For development with live SuperSonic changes, symlink priv/static/supersonic -> ../../../../supersonic/dist
  plug Plug.Static,
    at: "/",
    from: :tau5,
    gzip: false,
    only: Tau5Web.static_paths()

  # Code reloading can be explicitly enabled under the
  # :code_reloader configuration of your endpoint.
  if code_reloading? do
    socket "/phoenix/live_reload/socket", Phoenix.LiveReloader.Socket
    plug Phoenix.LiveReloader
    plug Phoenix.CodeReloader
  end

  plug Phoenix.LiveDashboard.RequestLogger,
    param_key: "request_logger",
    cookie_key: "request_logger"

  plug Plug.RequestId
  plug Plug.Telemetry, event_prefix: [:phoenix, :endpoint]

  plug Plug.Parsers,
    parsers: [:urlencoded, :multipart, :json],
    pass: ["*/*"],
    json_decoder: Phoenix.json_library()

  plug Plug.MethodOverride
  plug Plug.Head
  plug Plug.Session, @session_options

  # Security for internal endpoint - requires localhost + app token
  plug Tau5Web.Plugs.InternalEndpointSecurity

  plug Tau5Web.Router
end
