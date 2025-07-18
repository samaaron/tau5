defmodule Tau5MCP do
  @moduledoc """
  Tau5 MCP Plug - Provides Model Context Protocol endpoints for tau5.
  """
  
  @behaviour Plug
  
  alias Hermes.Server.Transport.SSE
  alias Plug.Conn
  
  def init(opts), do: opts
  
  def call(%Conn{request_path: "/tau5/mcp", method: "GET"} = conn, _opts) do
    opts = SSE.Plug.init(server: Tau5MCP.Server, mode: :sse)
    SSE.Plug.call(conn, opts)
  end
  
  def call(%Conn{request_path: "/tau5/mcp/message", method: "POST"} = conn, _opts) do
    opts = SSE.Plug.init(server: Tau5MCP.Server, mode: :post)
    SSE.Plug.call(conn, opts)
  end
  
  def call(conn, _opts), do: conn
end