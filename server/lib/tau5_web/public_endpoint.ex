defmodule Tau5Web.PublicEndpoint do
  use Phoenix.Endpoint, otp_app: :tau5

  # The session will be stored in the cookie and signed,
  # this means its contents can be read but not tampered with.
  # Set :encryption_salt if you would also like to encrypt it.
  @session_options [
    store: :cookie,
    key: "_tau5_public_key",
    signing_salt: "KaBar2nliI8=",
    same_site: "Lax",
    http_only: true  # Prevent JavaScript access to cookies
    # Note: secure flag omitted as this may run on local networks without HTTPS
  ]

  socket "/live", Phoenix.LiveView.Socket,
    websocket: [connect_info: [session: @session_options, peer_data: true, x_headers: ["x-forwarded-for"], uri: true]],
    longpoll: [connect_info: [session: @session_options, peer_data: true, x_headers: ["x-forwarded-for"], uri: true]]

  # Serve at "/" the static files from "priv/static" directory.
  #
  # You should set gzip to true if you are running phx.digest
  # when deploying your static files in production.
  plug Plug.Static,
    at: "/",
    from: :tau5,
    gzip: false,
    only: Tau5Web.static_paths()

  # Code reloading in development
  if code_reloading? do
    socket "/phoenix/live_reload/socket", Phoenix.LiveReloader.Socket
    plug Phoenix.LiveReloader
    plug Phoenix.CodeReloader
  end
  
  plug Plug.RequestId
  plug Plug.Telemetry, event_prefix: [:phoenix, :public_endpoint]

  plug Plug.Parsers,
    parsers: [:urlencoded, :multipart, :json],
    pass: ["*/*"],
    json_decoder: Phoenix.json_library()

  plug Plug.MethodOverride
  plug Plug.Head
  plug Plug.Session, @session_options
  
  # Friend authentication - must come after session but before access control
  plug Tau5Web.Plugs.FriendAuthentication
  
  # Access control plug - must come before router
  plug Tau5Web.Plugs.PublicAccessControl
  
  plug Tau5Web.Router
end