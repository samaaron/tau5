defmodule Tau5Web.MCPEndpoint do
  @moduledoc """
  Dedicated endpoint for MCP (Model Context Protocol) servers.
  This endpoint runs on a configurable port (default 5555) and serves only MCP traffic.
  """
  use Phoenix.Endpoint, otp_app: :tau5

  @session_options [
    store: :cookie,
    key: "_tau5_mcp_key",
    signing_salt: "McpSigningSalt123",
    same_site: "Lax",
    http_only: true
  ]

  if code_reloading? do
    plug Phoenix.CodeReloader
  end

  plug Plug.RequestId
  plug Plug.Telemetry, event_prefix: [:phoenix, :mcp_endpoint]

  plug Plug.Parsers,
    parsers: [:urlencoded, :multipart, :json],
    pass: ["*/*"],
    json_decoder: Phoenix.json_library(),
    body_reader: {Tau5Web.Plugs.CachingBodyReader, :read_body, []},
    length: 10_000_000

  plug Plug.MethodOverride
  plug Plug.Head
  plug Plug.Session, @session_options

  plug Tau5Web.MCPRouter
end
