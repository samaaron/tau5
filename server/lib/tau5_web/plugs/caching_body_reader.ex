defmodule Tau5Web.Plugs.CachingBodyReader do
  @moduledoc """
  A body reader that caches the request body for multiple reads.
  This is necessary for MCP servers that may need to read the body multiple times.
  """

  def read_body(conn, opts \\ []) do
    case Plug.Conn.read_body(conn, opts) do
      {:ok, body, conn} ->
        conn = Plug.Conn.put_private(conn, :raw_body, body)
        {:ok, body, conn}
      
      {:more, partial, conn} ->
        {:more, partial, conn}
      
      {:error, reason} ->
        {:error, reason}
    end
  end
end