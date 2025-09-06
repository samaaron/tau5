defmodule Tau5Web.MCPFallbackController do
  use Tau5Web, :controller

  def not_found(conn, _params) do
    conn
    |> put_status(404)
    |> json(%{error: "MCP route not found"})
  end
end
