defmodule Tau5Web.Plugs.PublicAccessControl do
  @moduledoc """
  Plug that controls access to the public endpoint based on the enabled/disabled state.
  Local connections are always allowed, remote connections are filtered.
  """
  import Plug.Conn
  require Logger

  def init(opts), do: opts

  def call(conn, _opts) do
    if local_connection?(conn) or Tau5.PublicEndpoint.enabled?() do
      conn
    else
      Logger.debug("PublicAccessControl: Rejecting remote connection from #{inspect(conn.remote_ip)}")
      
      conn
      |> put_status(503)
      |> put_resp_content_type("text/plain")
      |> send_resp(503, "Remote access is currently disabled")
      |> halt()
    end
  end

  defp local_connection?(conn) do
    case conn.remote_ip do
      {127, 0, 0, 1} -> true
      {0, 0, 0, 0, 0, 0, 0, 1} -> true
      {0, 0, 0, 0, 0, 65535, 32512, 1} -> true  # ::ffff:127.0.0.1
      {127, _, _, _} -> true  # 127.0.0.0/8
      _ -> false
    end
  end
end