defmodule Tau5Web.AccessTierHook do
  @moduledoc """
  LiveView hook that ensures access tier information is available in socket assigns.
  Since we can't access conn directly from LiveView, we use the session to pass
  friend authentication status from the plug.
  """
  
  import Phoenix.Component, only: [assign: 3]
  alias Tau5Web.AccessTier
  
  def on_mount(:default, _params, session, socket) do
    try do
      require Logger
      
      if Application.get_env(:tau5, :dev_routes, false) do
        Logger.debug("AccessTierHook: socket keys = #{inspect(Map.keys(socket))}")
        Logger.debug("AccessTierHook: session keys = #{inspect(Map.keys(session))}")
      end
      
      pseudo_conn = if session["friend_authenticated"] do
        %{assigns: %{friend_authenticated: true}}
      else
        nil
      end
      
      endpoint_module = cond do
        is_map(socket) && Map.has_key?(socket, :endpoint) && 
        not is_nil(Map.get(socket, :endpoint)) -> Map.get(socket, :endpoint)
        session["endpoint"] == "public" -> Tau5Web.PublicEndpoint
        true -> Tau5Web.Endpoint
      end
      
      if Application.get_env(:tau5, :dev_routes, false) do
        Logger.debug("AccessTierHook: detected endpoint_module = #{inspect(endpoint_module)}")
      end
      
      {endpoint_type, access_tier, features} = 
        AccessTier.get_access_info(endpoint_module, pseudo_conn)
      
      {:cont, 
       socket
       |> assign(:endpoint_type, endpoint_type)
       |> assign(:access_tier, access_tier)
       |> assign(:features, features)}
    rescue
      error ->
        require Logger
        Logger.error("AccessTierHook failed: #{inspect(error)}")
        Logger.error("Socket structure: #{inspect(socket)}")
        Logger.error("Session structure: #{inspect(session)}")
        Logger.error("Stack: #{inspect(__STACKTRACE__)}")
        
        {:cont, 
         socket
         |> assign(:endpoint_type, "local")
         |> assign(:access_tier, "full")
         |> assign(:features, %{
           admin_tools: true,
           pairing: true,
           fs_access: true,
           mutate: true,
           console_access: true,
           lua_privileged: true,
           midi_access: true,
           link_access: true
         })}
    end
  end
end