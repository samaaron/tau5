defmodule Tau5Web.AccessTier do
  @moduledoc """
  Shared logic for determining endpoint type and access tier.
  Separates the concept of which endpoint was used (local/public)
  from the actual access level granted.
  """

  @doc """
  Returns {endpoint_type, access_tier, features}

  - endpoint_type: "local" | "public" - which endpoint the request came through
  - access_tier: "full" | "friend" | "restricted" - the actual permission level
  - features: map of enabled features based on access tier
  """
  def get_access_info(endpoint, conn \\ nil) do
    endpoint_type = get_endpoint_type(endpoint)
    {access_tier, features} = get_tier_for_endpoint(endpoint_type, conn)

    {endpoint_type, access_tier, features}
  end

  defp get_endpoint_type(endpoint) do
    if endpoint == Tau5Web.PublicEndpoint do
      "public"
    else
      "local"
    end
  end

  defp get_tier_for_endpoint("public", conn) do
    # Check if this is a friend-authenticated connection
    if conn && Map.get(conn.assigns, :friend_authenticated, false) do
      get_friend_tier()
    else
      get_restricted_tier()
    end
  end

  defp get_tier_for_endpoint("local", _conn) do
    {"full",
     %{
       admin_tools: true,
       console_access: true,
       lua_privileged: true,
     }}
  end

  defp get_friend_tier do
    {"friend",
     %{
       console_access: true,
       lua_privileged: true
     }}
  end

  defp get_restricted_tier do
    {"restricted",
     %{
       console_access: false,
       lua_privileged: false
     }}
  end
end
