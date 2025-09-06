defmodule Tau5MCP do
  @moduledoc """
  Tau5 MCP Plug - Provides Model Context Protocol endpoints for tau5.

  Uses Streamable HTTP transport which supports both JSON responses and SSE streaming.
  """

  @behaviour Plug

  alias Hermes.Server.Transport.StreamableHTTP
  alias Plug.Conn

  def init(opts), do: opts

  def call(%Conn{request_path: "/tau5/mcp"} = conn, _opts) do
    # Use Streamable HTTP transport which handles GET, POST, and DELETE
    opts = StreamableHTTP.Plug.init(server: Tau5MCP.Server)
    StreamableHTTP.Plug.call(conn, opts)
  end

  def call(conn, _opts), do: conn
end
