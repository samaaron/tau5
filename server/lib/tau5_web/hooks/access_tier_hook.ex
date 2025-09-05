defmodule Tau5Web.AccessTierHook do
  @moduledoc """
  LiveView hook that ensures access tier information is available in socket assigns.
  Since we can't access conn directly from LiveView, we use the session to pass
  friend authentication status from the plug.
  """
  
  import Phoenix.Component, only: [assign: 3]
  alias Tau5Web.AccessTier
  
  def on_mount(:default, _params, session, socket) do
    # Create a pseudo-conn with friend auth status from session
    pseudo_conn = if session["friend_authenticated"] do
      %{assigns: %{friend_authenticated: true}}
    else
      nil
    end
    
    # Get the access tier using our pseudo conn
    {endpoint_type, access_tier, features} = 
      AccessTier.get_access_info(socket.endpoint, pseudo_conn)
    
    {:cont, 
     socket
     |> assign(:endpoint_type, endpoint_type)
     |> assign(:access_tier, access_tier)
     |> assign(:features, features)}
  end
end