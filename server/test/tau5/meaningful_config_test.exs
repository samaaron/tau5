defmodule Tau5.MeaningfulConfigTest do
  use Tau5Web.ConnCase
  alias Tau5Web.Plugs.ConsoleSecurity
  
  describe "TAU5_ENABLE_DEV_REPL actually controls console access" do
    test "console is blocked when Application config says disabled" do
      # This tests the ACTUAL behavior - the plug checks Application.get_env
      Application.delete_env(:tau5, :console_enabled)
      System.put_env("TAU5_SESSION_TOKEN", "valid-token")
      
      conn = 
        build_conn(:get, "/dev/console?token=valid-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      # Verify it's actually blocked by the console_enabled check
      assert result.halted
      assert result.status == 403
      assert result.resp_body =~ "The Tau5 Elixir REPL console is disabled"
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end
    
    test "console is allowed when Application config says enabled" do
      Application.put_env(:tau5, :console_enabled, true)
      System.put_env("TAU5_SESSION_TOKEN", "valid-token")
      
      conn = 
        build_conn(:get, "/dev/console?token=valid-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      # Verify it passes all checks
      refute result.halted
      
      Application.delete_env(:tau5, :console_enabled)
      System.delete_env("TAU5_SESSION_TOKEN")
    end
  end
  
  describe "Localhost validation actually works" do
    setup do
      Application.put_env(:tau5, :console_enabled, true)
      System.put_env("TAU5_SESSION_TOKEN", "token")
      on_exit(fn -> 
        Application.delete_env(:tau5, :console_enabled)
        System.delete_env("TAU5_SESSION_TOKEN")
      end)
      :ok
    end
    
    test "127.0.0.1 is actually accepted as localhost" do
      conn = 
        build_conn(:get, "/dev/console?token=token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      refute result.halted
    end
    
    test "192.168.1.1 is actually rejected as non-localhost" do
      conn = 
        build_conn(:get, "/dev/console?token=token")
        |> Map.put(:remote_ip, {192, 168, 1, 1})
      
      result = ConsoleSecurity.call(conn, [])
      assert result.halted
      assert result.resp_body =~ "does not accept remote connections"
    end
    
    test "127.0.0.2 is accepted (same /24 subnet as 127.0.0.1)" do
      conn = 
        build_conn(:get, "/dev/console?token=token")
        |> Map.put(:remote_ip, {127, 0, 0, 2})
      
      result = ConsoleSecurity.call(conn, [])
      refute result.halted
    end
    
    test "127.0.1.1 is rejected (different /24 subnet)" do
      conn = 
        build_conn(:get, "/dev/console?token=token")
        |> Map.put(:remote_ip, {127, 0, 1, 1})
      
      result = ConsoleSecurity.call(conn, [])
      assert result.halted
    end
  end
  
  describe "Token validation actually validates tokens" do
    setup do
      Application.put_env(:tau5, :console_enabled, true)
      on_exit(fn -> Application.delete_env(:tau5, :console_enabled) end)
      :ok
    end
    
    test "correct token is accepted" do
      System.put_env("TAU5_SESSION_TOKEN", "my-secret-token")
      
      conn = 
        build_conn(:get, "/dev/console?token=my-secret-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      refute result.halted
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end
    
    test "wrong token is rejected" do
      System.put_env("TAU5_SESSION_TOKEN", "my-secret-token")
      
      conn = 
        build_conn(:get, "/dev/console?token=wrong-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      assert result.halted
      assert result.resp_body =~ "Invalid or missing session token"
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end
    
    test "missing token is rejected when server expects one" do
      System.put_env("TAU5_SESSION_TOKEN", "my-secret-token")
      
      conn = 
        build_conn(:get, "/dev/console")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      assert result.halted
      assert result.resp_body =~ "Invalid or missing session token"
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end
    
    test "server with no token configured rejects all access" do
      System.delete_env("TAU5_SESSION_TOKEN")
      
      conn = 
        build_conn(:get, "/dev/console?token=anything")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      assert result.halted
      assert result.resp_body =~ "Server not configured"
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end
  end
  
  describe "Security layers are independent" do
    test "all three checks must pass independently" do
      # Test every combination to ensure they're independent
      scenarios = [
        # {enabled, token_valid, is_local, should_pass}
        {true, true, true, true},    # All good - should pass
        {true, true, false, false},  # Not localhost
        {true, false, true, false},  # Bad token
        {true, false, false, false}, # Bad token AND not localhost
        {false, true, true, false},  # Console disabled
        {false, true, false, false}, # Console disabled AND not localhost
        {false, false, true, false}, # Console disabled AND bad token
        {false, false, false, false},# Everything wrong
      ]
      
      for {enabled, token_valid, is_local, should_pass} <- scenarios do
        # Setup
        if enabled do
          Application.put_env(:tau5, :console_enabled, true)
        else
          Application.delete_env(:tau5, :console_enabled)
        end
        
        System.put_env("TAU5_SESSION_TOKEN", "correct-token")
        
        token = if token_valid, do: "correct-token", else: "wrong-token"
        ip = if is_local, do: {127, 0, 0, 1}, else: {10, 0, 0, 1}
        
        conn = 
          build_conn(:get, "/dev/console?token=#{token}")
          |> Map.put(:remote_ip, ip)
        
        result = ConsoleSecurity.call(conn, [])
        
        if should_pass do
          refute result.halted, 
            "Should pass with enabled=#{enabled}, valid=#{token_valid}, local=#{is_local}"
        else
          assert result.halted,
            "Should fail with enabled=#{enabled}, valid=#{token_valid}, local=#{is_local}"
        end
        
        # Cleanup
        Application.delete_env(:tau5, :console_enabled)
        System.delete_env("TAU5_SESSION_TOKEN")
      end
    end
  end
  
  describe "Headers don't bypass security" do
    setup do
      Application.put_env(:tau5, :console_enabled, true)
      System.put_env("TAU5_SESSION_TOKEN", "token")
      on_exit(fn -> 
        Application.delete_env(:tau5, :console_enabled)
        System.delete_env("TAU5_SESSION_TOKEN")
      end)
      :ok
    end
    
    test "X-Forwarded-For header doesn't override remote_ip" do
      conn = 
        build_conn(:get, "/dev/console?token=token")
        |> put_req_header("x-forwarded-for", "127.0.0.1")
        |> Map.put(:remote_ip, {8, 8, 8, 8})  # Actual IP is external
      
      result = ConsoleSecurity.call(conn, [])
      
      # Should be blocked - header doesn't override actual IP
      assert result.halted
      assert result.resp_body =~ "does not accept remote connections"
    end
  end
  
  describe "Query parameter edge cases" do
    setup do
      Application.put_env(:tau5, :console_enabled, true)
      System.put_env("TAU5_SESSION_TOKEN", "token")
      on_exit(fn -> 
        Application.delete_env(:tau5, :console_enabled)
        System.delete_env("TAU5_SESSION_TOKEN")
      end)
      :ok
    end
    
    test "empty token parameter is treated as missing" do
      conn = 
        build_conn(:get, "/dev/console?token=")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      assert result.halted
      assert result.resp_body =~ "Invalid or missing session token"
    end
    
    test "multiple token parameters uses the last one (Plug behavior)" do
      # This is Plug's actual behavior - last param wins
      conn = 
        build_conn(:get, "/dev/console?token=wrong&token=token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      # Last token "token" matches, should pass
      refute result.halted
    end
  end
end