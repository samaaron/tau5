defmodule Tau5Web.AccessTier do
  @moduledoc """
  Shared logic for determining endpoint type and access tier.
  Separates the concept of which endpoint was used (local/public)
  from the actual access level granted.
  """
  
  @doc """
  Returns {endpoint_type, access_tier, features}
  
  - endpoint_type: "local" | "public" - which endpoint the request came through
  - access_tier: "full" | "restricted" - the actual permission level
  - features: map of enabled features based on access tier
  """
  def get_access_info(endpoint) do
    endpoint_type = get_endpoint_type(endpoint)
    {access_tier, features} = get_tier_for_endpoint(endpoint_type)
    
    {endpoint_type, access_tier, features}
  end
  
  defp get_endpoint_type(endpoint) do
    if endpoint == Tau5Web.PublicEndpoint do
      "public"
    else
      "local"
    end
  end
  
  defp get_tier_for_endpoint("public") do
    {"restricted", %{
      admin_tools: false,
      pairing: false,
      fs_access: false,
      mutate: true,
      console_access: false
    }}
  end
  
  defp get_tier_for_endpoint("local") do
    {"full", %{
      admin_tools: true,
      pairing: true,
      fs_access: true,
      mutate: true,
      console_access: true
    }}
  end
end