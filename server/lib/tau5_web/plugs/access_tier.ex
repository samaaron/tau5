defmodule Tau5Web.Plugs.AccessTier do
  @moduledoc """
  Plug that determines the access tier based on which endpoint is serving the request.
  Sets access_type, internal_access, and features in conn.assigns which automatically
  flow to socket.assigns in LiveViews.
  """
  import Plug.Conn
  alias Tau5Web.AccessTier

  def init(opts), do: opts

  def call(conn, _opts) do
    endpoint = conn.private.phoenix_endpoint
    {endpoint_type, access_tier, features} = AccessTier.get_access_info(endpoint)
    
    conn
    |> assign(:endpoint_type, endpoint_type)
    |> assign(:access_tier, access_tier)
    |> assign(:features, features)
  end
end