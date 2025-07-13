defmodule Tau5Web.Plugs.ConsoleSecurityTest do
  use Tau5Web.ConnCase
  alias Tau5Web.Plugs.ConsoleSecurity
  
  # Helper to build conn with query params
  defp build_conn_with_query(query_string) do
    build_conn(:get, "/dev/console?#{query_string}")
  end

  describe "localhost validation" do
    test "allows IPv4 localhost (127.0.0.1)", %{conn: _conn} do
      System.put_env("TAU5_SESSION_TOKEN", "test-token-123")
      
      conn = 
        build_conn_with_query("token=test-token-123")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      refute result.halted
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end

    test "allows IPv6 localhost (::1)", %{conn: _conn} do
      System.put_env("TAU5_SESSION_TOKEN", "test-token-123")
      
      conn = 
        build_conn_with_query("token=test-token-123")
        |> Map.put(:remote_ip, {0, 0, 0, 0, 0, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      refute result.halted
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end

    test "allows IPv4-mapped IPv6 localhost", %{conn: _conn} do
      System.put_env("TAU5_SESSION_TOKEN", "test-token-123")
      
      conn = 
        build_conn_with_query("token=test-token-123")
        |> Map.put(:remote_ip, {0, 0, 0, 0, 0, 65535, 32512, 1})
      
      result = ConsoleSecurity.call(conn, [])
      refute result.halted
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end

    test "blocks non-localhost IPv4 addresses", %{conn: _conn} do
      conn = 
        build_conn(:get, "/dev/console")
        |> Map.put(:remote_ip, {192, 168, 1, 100})
      
      result = ConsoleSecurity.call(conn, [])
      
      assert result.halted
      assert result.status == 403
      assert result.resp_body =~ "does not accept remote connections"
    end

    test "blocks non-localhost IPv6 addresses", %{conn: _conn} do
      conn = 
        build_conn(:get, "/dev/console")
        |> Map.put(:remote_ip, {8193, 3512, 0, 0, 0, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      assert result.halted
      assert result.status == 403
      assert result.resp_body =~ "does not accept remote connections"
    end

    test "blocks external IP addresses", %{conn: _conn} do
      external_ips = [
        {8, 8, 8, 8},          # Google DNS
        {1, 1, 1, 1},          # Cloudflare
        {172, 16, 0, 1},       # Private network
        {10, 0, 0, 1},         # Private network
        {192, 168, 0, 1}       # Private network
      ]
      
      for ip <- external_ips do
        conn = 
          build_conn(:get, "/dev/console")
          |> Map.put(:remote_ip, ip)
          
        result = ConsoleSecurity.call(conn, [])
        
        assert result.halted, "IP #{inspect(ip)} should be blocked"
        assert result.status == 403
      end
    end
  end

  describe "token validation" do
    setup do
      System.put_env("TAU5_SESSION_TOKEN", "valid-test-token")
      on_exit(fn -> System.delete_env("TAU5_SESSION_TOKEN") end)
      :ok
    end

    test "allows access with valid token", %{conn: _conn} do
      conn = 
        build_conn_with_query("token=valid-test-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      refute result.halted
    end

    test "blocks access with invalid token", %{conn: _conn} do
      conn = 
        build_conn_with_query("token=invalid-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      assert result.halted
      assert result.status == 403
      assert result.resp_body =~ "Invalid or missing session token"
    end

    test "blocks access with missing token", %{conn: _conn} do
      conn = 
        build_conn(:get, "/dev/console")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      assert result.halted
      assert result.status == 403
      assert result.resp_body =~ "Invalid or missing session token"
    end

    test "blocks access when server has no token configured", %{conn: _conn} do
      System.delete_env("TAU5_SESSION_TOKEN")
      
      conn = 
        build_conn_with_query("token=any-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      assert result.halted
      assert result.status == 403
      assert result.resp_body =~ "Server not configured for Tau5 console access"
    end
  end

  describe "path filtering" do
    test "ignores non-console paths", %{conn: _conn} do
      conn = 
        build_conn(:get, "/")
        |> Map.put(:remote_ip, {192, 168, 1, 100})  # Non-localhost
      
      result = ConsoleSecurity.call(conn, [])
      refute result.halted
    end

    test "only checks /dev/console path", %{conn: _conn} do
      paths_to_ignore = ["/", "/dev/dashboard", "/dev", "/console", "/dev/console/"]
      
      for path <- paths_to_ignore do
        conn = 
          build_conn(:get, path)
          |> Map.put(:remote_ip, {192, 168, 1, 100})  # Non-localhost
          
        result = ConsoleSecurity.call(conn, [])
        refute result.halted, "Path #{path} should not be checked"
      end
    end
  end

  describe "security combinations" do
    test "requires both localhost AND valid token", %{conn: _conn} do
      System.put_env("TAU5_SESSION_TOKEN", "valid-token")
      
      # Localhost but wrong token
      conn1 = 
        build_conn_with_query("token=wrong-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result1 = ConsoleSecurity.call(conn1, [])
      assert result1.halted, "Should block localhost with wrong token"
      
      # Valid token but not localhost
      conn2 = 
        build_conn_with_query("token=valid-token")
        |> Map.put(:remote_ip, {192, 168, 1, 1})
      
      result2 = ConsoleSecurity.call(conn2, [])
      assert result2.halted, "Should block non-localhost with valid token"
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end
  end
end