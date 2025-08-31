defmodule Tau5Web.MCPSecurityTest do
  use Tau5Web.ConnCase
  
  describe "MCP server security" do
    test "blocks access from public endpoint", %{conn: conn} do
      # Simulate request from public endpoint
      conn = %{conn | 
        private: %{phoenix_endpoint: Tau5Web.PublicEndpoint},
        request_path: "/tau5/mcp"
      }
      
      # Apply the MCP pipeline manually
      conn = conn
      |> Plug.Conn.put_req_header("accept", "application/json")
      |> Tau5Web.Plugs.RequireInternalEndpoint.call([])
      
      # Should be blocked with 404
      assert conn.halted
      assert conn.status == 404
    end
    
    test "allows access from internal endpoint", %{conn: conn} do
      # Simulate request from internal endpoint
      conn = %{conn | 
        private: %{phoenix_endpoint: Tau5Web.Endpoint},
        request_path: "/tau5/mcp"
      }
      
      # Apply the plug
      conn = conn
      |> Plug.Conn.put_req_header("accept", "application/json")
      |> Tau5Web.Plugs.RequireInternalEndpoint.call([])
      
      # Should not be blocked
      refute conn.halted
      assert conn.status == nil
    end
    
    test "MCP endpoints are protected in router", %{conn: conn} do
      # Test that the actual routes are protected
      # This ensures the pipeline is properly configured
      
      # From public endpoint - should return 403 (InternalEndpointSecurity blocks non-localhost)
      # Since tests run from localhost, we need to check the actual response
      public_conn = %{conn | private: %{phoenix_endpoint: Tau5Web.PublicEndpoint}}
      public_conn = get(public_conn, "/tau5/mcp")
      # In test environment with session token set, this returns 403 from InternalEndpointSecurity
      assert public_conn.status == 403
      
      # Note: We can't easily test the internal endpoint positive case here
      # because it requires the full MCP server to be running
    end
  end
end