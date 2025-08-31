defmodule Tau5Web.Plugs.AccessTierTest do
  use Tau5Web.ConnCase
  alias Tau5Web.Plugs.AccessTier
  
  describe "call/2 with internal endpoint" do
    test "sets internal access flags correctly", %{conn: conn} do
      # Simulate request from internal endpoint
      conn = %{conn | private: %{phoenix_endpoint: Tau5Web.Endpoint}}
      result = AccessTier.call(conn, [])
      
      # Check assigns
      assert result.assigns.endpoint_type == "local"
      assert result.assigns.access_tier == "full"
      
      # Check features
      features = result.assigns.features
      assert features.admin_tools == true
      assert features.pairing == true
      assert features.fs_access == true
      assert features.mutate == true
      assert features.console_access == true
    end
    
    test "allows full feature set for internal endpoint", %{conn: conn} do
      conn = %{conn | private: %{phoenix_endpoint: Tau5Web.Endpoint}}
      result = AccessTier.call(conn, [])
      
      features = result.assigns.features
      # All features should be enabled
      assert Enum.all?(Map.values(features), & &1)
    end
  end
  
  describe "call/2 with public endpoint" do
    test "sets public access flags correctly", %{conn: conn} do
      # Simulate request from public endpoint
      conn = %{conn | private: %{phoenix_endpoint: Tau5Web.PublicEndpoint}}
      result = AccessTier.call(conn, [])
      
      # Check assigns
      assert result.assigns.endpoint_type == "public"
      assert result.assigns.access_tier == "restricted"
      
      # Check features
      features = result.assigns.features
      assert features.admin_tools == false
      assert features.pairing == false
      assert features.fs_access == false
      assert features.mutate == true  # Only this should be true
      assert features.console_access == false
    end
    
    test "restricts dangerous features for public endpoint", %{conn: conn} do
      conn = %{conn | private: %{phoenix_endpoint: Tau5Web.PublicEndpoint}}
      result = AccessTier.call(conn, [])
      
      features = result.assigns.features
      # Security-sensitive features should be disabled
      refute features.admin_tools
      refute features.fs_access
      refute features.console_access
      refute features.pairing
      
      # But mutate should still work for collaboration
      assert features.mutate
    end
  end
  
  describe "feature differentiation" do
    test "internal endpoint has more features than public", %{conn: conn} do
      # Internal endpoint
      internal_conn = %{conn | private: %{phoenix_endpoint: Tau5Web.Endpoint}}
      internal_result = AccessTier.call(internal_conn, [])
      internal_features = internal_result.assigns.features
      
      # Public endpoint  
      public_conn = %{conn | private: %{phoenix_endpoint: Tau5Web.PublicEndpoint}}
      public_result = AccessTier.call(public_conn, [])
      public_features = public_result.assigns.features
      
      # Count enabled features
      internal_count = Enum.count(Map.values(internal_features), & &1)
      public_count = Enum.count(Map.values(public_features), & &1)
      
      assert internal_count > public_count
      assert internal_count == 5  # All features
      assert public_count == 1    # Only mutate
    end
  end
  
  describe "assigns propagation" do
    test "assigns are available for downstream plugs", %{conn: conn} do
      conn = %{conn | private: %{phoenix_endpoint: Tau5Web.Endpoint}}
      result = AccessTier.call(conn, [])
      
      # All assigns should be set
      assert Map.has_key?(result.assigns, :endpoint_type)
      assert Map.has_key?(result.assigns, :access_tier)
      assert Map.has_key?(result.assigns, :features)
      
      # They should have the expected types
      assert is_binary(result.assigns.endpoint_type)
      assert is_binary(result.assigns.access_tier)
      assert is_map(result.assigns.features)
    end
  end
end