defmodule Tau5.ConfigTest do
  use Tau5Web.ConnCase
  alias Tau5Web.Plugs.ConsoleSecurity
  
  describe "TAU5_ENABLE_DEV_REPL integration" do
    test "console security plug blocks access when REPL is disabled", %{conn: _conn} do
      # Ensure REPL is disabled in application config
      Application.delete_env(:tau5, :console_enabled)
      System.put_env("TAU5_SESSION_TOKEN", "valid-token")
      
      conn = 
        build_conn(:get, "/dev/console?token=valid-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      # Should be blocked even with valid token and localhost
      assert result.halted
      assert result.status == 403
      assert result.resp_body =~ "The Tau5 Elixir REPL console is disabled"
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end
    
    test "console security plug allows access when REPL is enabled with proper auth", %{conn: _conn} do
      # Enable REPL in application config
      Application.put_env(:tau5, :console_enabled, true)
      System.put_env("TAU5_SESSION_TOKEN", "valid-token")
      
      conn = 
        build_conn(:get, "/dev/console?token=valid-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      # Should allow access with valid token, localhost, and enabled REPL
      refute result.halted
      
      Application.delete_env(:tau5, :console_enabled)
      System.delete_env("TAU5_SESSION_TOKEN")
    end
    
    test "console security still enforces localhost requirement when REPL is enabled", %{conn: _conn} do
      # Enable REPL but try from non-localhost
      Application.put_env(:tau5, :console_enabled, true)
      System.put_env("TAU5_SESSION_TOKEN", "valid-token")
      
      conn = 
        build_conn(:get, "/dev/console?token=valid-token")
        |> Map.put(:remote_ip, {192, 168, 1, 100})
      
      result = ConsoleSecurity.call(conn, [])
      
      # Should still be blocked due to non-localhost even with REPL enabled
      assert result.halted
      assert result.status == 403
      assert result.resp_body =~ "does not accept remote connections"
      
      Application.delete_env(:tau5, :console_enabled)
      System.delete_env("TAU5_SESSION_TOKEN")
    end
    
    test "console security still enforces token requirement when REPL is enabled", %{conn: _conn} do
      # Enable REPL but provide wrong token
      Application.put_env(:tau5, :console_enabled, true)
      System.put_env("TAU5_SESSION_TOKEN", "valid-token")
      
      conn = 
        build_conn(:get, "/dev/console?token=wrong-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      # Should still be blocked due to invalid token even with REPL enabled
      assert result.halted
      assert result.status == 403
      assert result.resp_body =~ "Invalid or missing session token"
      
      Application.delete_env(:tau5, :console_enabled)
      System.delete_env("TAU5_SESSION_TOKEN")
    end
  end
  
  
  describe "security configuration defaults" do
    test "accessing /dev/console without any configuration returns security error" do
      # Clear all configuration
      Application.delete_env(:tau5, :console_enabled)
      System.delete_env("TAU5_SESSION_TOKEN")
      System.delete_env("TAU5_ENABLE_DEV_REPL")
      
      conn = 
        build_conn(:get, "/dev/console")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      # Should be blocked with appropriate security message
      assert result.halted
      assert result.status == 403
      # First check should be console_enabled, which fails
      assert result.resp_body =~ "disabled"
      
      # Cleanup
      Application.delete_env(:tau5, :console_enabled)
    end
    
    test "console requires all three security checks to pass" do
      # This is a comprehensive test of the security layers
      test_cases = [
        # {console_enabled, has_token, is_localhost, should_pass}
        {false, false, false, false},  # Nothing enabled
        {false, false, true, false},   # Only localhost
        {false, true, false, false},   # Only token
        {false, true, true, false},    # Token + localhost, but console disabled
        {true, false, false, false},   # Only console enabled
        {true, false, true, false},    # Console + localhost, no token
        {true, true, false, false},    # Console + token, not localhost
        {true, true, true, true},      # All three checks pass
      ]
      
      for {console_enabled, has_token, is_localhost, should_pass} <- test_cases do
        # Set up the configuration
        if console_enabled do
          Application.put_env(:tau5, :console_enabled, true)
        else
          Application.delete_env(:tau5, :console_enabled)
        end
        
        query_string = if has_token do
          System.put_env("TAU5_SESSION_TOKEN", "test-token")
          "token=test-token"
        else
          System.delete_env("TAU5_SESSION_TOKEN")
          ""
        end
        
        ip = if is_localhost, do: {127, 0, 0, 1}, else: {192, 168, 1, 1}
        
        url = if query_string == "", do: "/dev/console", else: "/dev/console?#{query_string}"
        conn = 
          build_conn(:get, url)
          |> Map.put(:remote_ip, ip)
        
        result = ConsoleSecurity.call(conn, [])
        
        if should_pass do
          refute result.halted, 
            "Expected to pass with console=#{console_enabled}, token=#{has_token}, localhost=#{is_localhost}"
        else
          assert result.halted,
            "Expected to fail with console=#{console_enabled}, token=#{has_token}, localhost=#{is_localhost}"
          assert result.status == 403
        end
        
        # Cleanup
        Application.delete_env(:tau5, :console_enabled)
        System.delete_env("TAU5_SESSION_TOKEN")
      end
    end
  end
end