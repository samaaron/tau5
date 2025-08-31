defmodule Tau5Web.Plugs.PublicAccessControlTest do
  use Tau5Web.ConnCase
  alias Tau5Web.Plugs.PublicAccessControl
  
  setup do
    # The PublicEndpoint GenServer might be started by the application
    # If not running, start it; if running, just reset its state
    case Process.whereis(Tau5.PublicEndpoint) do
      nil -> 
        # Start it if not running - this happens in some test environments
        start_supervised!({Tau5.PublicEndpoint, [enabled: false]})
      _pid -> 
        # Reset to disabled state if already running
        Tau5.PublicEndpoint.disable()
    end
    :ok
  end
  
  describe "call/2 with local connections" do
    test "allows IPv4 localhost", %{conn: conn} do
      conn = %{conn | remote_ip: {127, 0, 0, 1}}
      result = PublicAccessControl.call(conn, [])
      
      # Should not halt the connection
      refute result.halted
      assert result.status == nil
    end
    
    test "allows IPv6 localhost", %{conn: conn} do
      conn = %{conn | remote_ip: {0, 0, 0, 0, 0, 0, 0, 1}}
      result = PublicAccessControl.call(conn, [])
      
      # Should not halt the connection
      refute result.halted
      assert result.status == nil
    end
    
    test "allows IPv4-mapped IPv6 localhost", %{conn: conn} do
      conn = %{conn | remote_ip: {0, 0, 0, 0, 0, 65535, 32512, 1}}
      result = PublicAccessControl.call(conn, [])
      
      # Should not halt the connection
      refute result.halted
      assert result.status == nil
    end
    
    test "blocks IPv6 link-local addresses for security", %{conn: conn} do
      # Link-local addresses are no longer considered local for security reasons
      conn = %{conn | remote_ip: {0xfe80, 0, 0, 0, 0x1234, 0x5678, 0x9abc, 0xdef0}}
      
      # Disable public endpoint to ensure it blocks
      Tau5.PublicEndpoint.disable()
      
      result = PublicAccessControl.call(conn, [])
      
      # Should halt the connection
      assert result.halted
      assert result.status == 503
    end
    
    test "allows any 127.x.x.x address", %{conn: conn} do
      conn = %{conn | remote_ip: {127, 0, 0, 5}}
      result = PublicAccessControl.call(conn, [])
      
      # Should not halt the connection
      refute result.halted
      assert result.status == nil
    end
  end
  
  describe "call/2 with remote connections when disabled" do
    setup do
      # Ensure remote access is disabled
      Tau5.PublicEndpoint.disable()
      :ok
    end
    
    test "blocks IPv4 remote connection", %{conn: conn} do
      conn = %{conn | remote_ip: {192, 168, 1, 100}}
      result = PublicAccessControl.call(conn, [])
      
      # Should halt the connection with 503
      assert result.halted
      assert result.status == 503
      assert result.resp_body == "Remote access is currently disabled"
    end
    
    test "blocks IPv6 remote connection", %{conn: conn} do
      conn = %{conn | remote_ip: {0x2001, 0xdb8, 0, 0, 0, 0, 0, 1}}
      result = PublicAccessControl.call(conn, [])
      
      # Should halt the connection with 503
      assert result.halted
      assert result.status == 503
      assert result.resp_body == "Remote access is currently disabled"
    end
    
    test "blocks public IPv4 addresses", %{conn: conn} do
      # Test with Google's DNS
      conn = %{conn | remote_ip: {8, 8, 8, 8}}
      result = PublicAccessControl.call(conn, [])
      
      assert result.halted
      assert result.status == 503
    end
  end
  
  describe "call/2 with remote connections when enabled" do
    setup do
      # Enable remote access
      Tau5.PublicEndpoint.enable()
      :ok
    end
    
    test "allows IPv4 remote connection", %{conn: conn} do
      conn = %{conn | remote_ip: {192, 168, 1, 100}}
      result = PublicAccessControl.call(conn, [])
      
      # Should not halt the connection
      refute result.halted
      assert result.status == nil
    end
    
    test "allows IPv6 remote connection", %{conn: conn} do
      conn = %{conn | remote_ip: {0x2001, 0xdb8, 0, 0, 0, 0, 0, 1}}
      result = PublicAccessControl.call(conn, [])
      
      # Should not halt the connection
      refute result.halted
      assert result.status == nil
    end
    
    test "allows public IPv4 addresses", %{conn: conn} do
      conn = %{conn | remote_ip: {8, 8, 8, 8}}
      result = PublicAccessControl.call(conn, [])
      
      refute result.halted
      assert result.status == nil
    end
  end
  
  describe "state transitions" do
    test "connection handling changes with state", %{conn: conn} do
      remote_conn = %{conn | remote_ip: {192, 168, 1, 100}}
      
      # Initially disabled - should block
      Tau5.PublicEndpoint.disable()
      result1 = PublicAccessControl.call(remote_conn, [])
      assert result1.halted
      
      # Enable - should allow
      Tau5.PublicEndpoint.enable()
      result2 = PublicAccessControl.call(remote_conn, [])
      refute result2.halted
      
      # Disable again - should block
      Tau5.PublicEndpoint.disable()
      result3 = PublicAccessControl.call(remote_conn, [])
      assert result3.halted
    end
  end
end