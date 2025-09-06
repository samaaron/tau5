defmodule Tau5Web.Plugs.RequireDesktopMode do
  @moduledoc """
  Plug that ensures MCP endpoints are only accessible in GUI mode.
  Returns 404 for all other deployment modes (node, central).
  """

  import Plug.Conn

  def init(opts), do: opts

  def call(conn, _opts) do
    case Application.get_env(:tau5, :deployment_mode, :node) do
      :gui ->
        # Allow access in GUI mode
        conn

      _other_mode ->
        # Block access in node/central modes
        conn
        |> send_resp(404, "Not Found")
        |> halt()
    end
  end
end
