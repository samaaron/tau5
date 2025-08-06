defmodule Tau5Web.Plugs.RequireDesktopMode do
  @moduledoc """
  Plug that ensures MCP endpoints are only accessible in desktop mode.
  Returns 404 for all other deployment modes (central, headless).
  """
  
  import Plug.Conn
  
  def init(opts), do: opts
  
  def call(conn, _opts) do
    case Application.get_env(:tau5, :deployment_mode, :headless) do
      :desktop ->
        # Allow access in desktop mode
        conn
        
      _other_mode ->
        # Block access in central/headless modes
        conn
        |> send_resp(404, "Not Found")
        |> halt()
    end
  end
end