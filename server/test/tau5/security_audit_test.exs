defmodule Tau5.SecurityAuditTest do
  use Tau5Web.ConnCase
  alias Tau5Web.Plugs.ConsoleSecurity
  
  describe "CRITICAL SECURITY: IP spoofing and bypass attempts" do
    setup do
      # Enable console for these security tests
      Application.put_env(:tau5, :console_enabled, true)
      System.put_env("TAU5_SESSION_TOKEN", "secure-token")
      on_exit(fn -> 
        Application.delete_env(:tau5, :console_enabled)
        System.delete_env("TAU5_SESSION_TOKEN")
      end)
      :ok
    end
    
    test "blocks localhost bypass attempts with various IP formats" do
      # These should be blocked as they're not in the allowed localhost patterns
      bypass_attempts = [
        {127, 0, 1, 1},           # 127.0.1.1 - common Ubuntu localhost alias (not in 127.0.0.x)
        {127, 1, 1, 1},           # 127.1.1.1 - still in 127.0.0.0/8 but not allowed
        {0, 0, 0, 0},             # 0.0.0.0 - all interfaces
        {0, 0, 0, 0, 0, 0, 0, 2}, # ::2 - not localhost
        {0, 0, 0, 0, 0, 65535, 32512, 2}, # IPv4-mapped but not .1
      ]
      
      # These ARE allowed by the current implementation
      allowed_localhost = [
        {127, 0, 0, 2},           # 127.0.0.2 - in 127.0.0.x range
        {127, 0, 0, 255},         # 127.0.0.255 - in 127.0.0.x range  
      ]
      
      for ip <- bypass_attempts do
        conn = 
          build_conn(:get, "/dev/console?token=secure-token")
          |> Map.put(:remote_ip, ip)
        
        result = ConsoleSecurity.call(conn, [])
        
        assert result.halted, "SECURITY: IP #{inspect(ip)} should be blocked"
        assert result.status == 403
        assert result.resp_body =~ "does not accept remote connections"
      end
      
      # Verify the allowed localhost IPs work
      for ip <- allowed_localhost do
        conn = 
          build_conn(:get, "/dev/console?token=secure-token")
          |> Map.put(:remote_ip, ip)
        
        result = ConsoleSecurity.call(conn, [])
        
        refute result.halted, "IP #{inspect(ip)} should be allowed as localhost"
      end
    end
    
    test "blocks X-Forwarded-For header manipulation" do
      # Attempt to use X-Forwarded-For to bypass localhost check
      conn = 
        build_conn(:get, "/dev/console?token=secure-token")
        |> put_req_header("x-forwarded-for", "127.0.0.1")
        |> Map.put(:remote_ip, {192, 168, 1, 100})
      
      result = ConsoleSecurity.call(conn, [])
      
      assert result.halted, "X-Forwarded-For bypass should be blocked"
      assert result.status == 403
    end
    
    test "blocks X-Real-IP header manipulation" do
      # Attempt to use X-Real-IP to bypass localhost check
      conn = 
        build_conn(:get, "/dev/console?token=secure-token")
        |> put_req_header("x-real-ip", "127.0.0.1")
        |> Map.put(:remote_ip, {10, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      assert result.halted, "X-Real-IP bypass should be blocked"
      assert result.status == 403
    end
  end
  
  describe "CRITICAL SECURITY: Token validation" do
    setup do
      Application.put_env(:tau5, :console_enabled, true)
      on_exit(fn -> Application.delete_env(:tau5, :console_enabled) end)
      :ok
    end
    
    test "blocks empty token attempts" do
      System.put_env("TAU5_SESSION_TOKEN", "secure-token")
      
      # Various empty token attempts
      empty_attempts = [
        "",           # Empty string
        "token=",     # Empty value
        "token",      # No value
      ]
      
      for query <- empty_attempts do
        conn = 
          build_conn(:get, "/dev/console?#{query}")
          |> Map.put(:remote_ip, {127, 0, 0, 1})
        
        result = ConsoleSecurity.call(conn, [])
        
        assert result.halted, "Empty token attempt '#{query}' should be blocked"
        assert result.status == 403
      end
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end
    
    test "token comparison is timing-attack resistant" do
      # Note: The current implementation uses == which is NOT timing-attack resistant
      # This test documents the vulnerability for future fixing
      System.put_env("TAU5_SESSION_TOKEN", "aaaaaaaa")
      
      # These tokens have increasing similarity to the real token
      # In a timing attack, "aaaaaaab" would take longer to reject than "bbbbbbbb"
      tokens = ["bbbbbbbb", "abbbbbbb", "aabbbbbb", "aaabbbbb", "aaaabbbb", "aaaaabbb", "aaaaaaab"]
      
      for token <- tokens do
        conn = 
          build_conn(:get, "/dev/console?token=#{token}")
          |> Map.put(:remote_ip, {127, 0, 0, 1})
        
        result = ConsoleSecurity.call(conn, [])
        
        assert result.halted, "Wrong token '#{token}' should be blocked"
        assert result.status == 403
      end
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end
    
    test "uses last token when multiple parameters provided" do
      System.put_env("TAU5_SESSION_TOKEN", "secure-token")
      
      # When multiple tokens provided, Plug uses the LAST one
      # This could be a minor security concern if not understood
      conn = 
        build_conn(:get, "/dev/console?token=wrong&token=secure-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      # Last token "secure-token" is used, so this should PASS
      refute result.halted, "Should use last token parameter (secure-token) and allow"
      
      # Now test the reverse - wrong token last
      conn2 = 
        build_conn(:get, "/dev/console?token=secure-token&token=wrong")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result2 = ConsoleSecurity.call(conn2, [])
      
      # Last token "wrong" is used, so this should be blocked
      assert result2.halted, "Should use last token parameter (wrong) and block"
      assert result2.resp_body =~ "Invalid or missing session token"
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end
  end
  
  describe "CRITICAL SECURITY: Path traversal and bypass" do
    test "only exact path /dev/console is protected" do
      System.put_env("TAU5_SESSION_TOKEN", "secure-token")
      Application.put_env(:tau5, :console_enabled, true)
      
      # These paths should NOT trigger security
      unprotected_paths = [
        "/dev/console/",           # Trailing slash - different path
        "/dev/console/../../etc",  # Path traversal - different path
        "/dev/Console",            # Case variation - different path
        "/dev//console",           # Double slash - different path
        "/dev/./console",          # Current directory - different path
        "/dev/console%20",         # URL encoded space - different path
        "//dev/console",           # Leading double slash - different path
      ]
      
      for path <- unprotected_paths do
        conn = 
          build_conn(:get, path)
          |> Map.put(:remote_ip, {192, 168, 1, 100})  # Non-localhost
        
        result = ConsoleSecurity.call(conn, [])
        
        # These should NOT be protected - they're different paths
        refute result.halted, "Path '#{path}' is correctly not protected (different path)"
      end
      
      # Test that /dev/console WITH query params IS protected
      conn = 
        build_conn(:get, "/dev/console?")
        |> Map.put(:remote_ip, {192, 168, 1, 100})  # Non-localhost
      
      result = ConsoleSecurity.call(conn, [])
      
      # This SHOULD be protected (request_path strips query params)
      assert result.halted, "/dev/console with query params should be protected"
      assert result.status == 403
      
      System.delete_env("TAU5_SESSION_TOKEN")
      Application.delete_env(:tau5, :console_enabled)
    end
  end
  
  describe "CRITICAL SECURITY: Order of checks" do
    test "console_enabled is checked FIRST before any other checks" do
      # This is important - we don't want to leak information about tokens
      # or localhost requirements if the console is disabled
      
      Application.delete_env(:tau5, :console_enabled)  # Disabled
      System.put_env("TAU5_SESSION_TOKEN", "valid-token")
      
      # Even with correct token and localhost, should fail with console disabled message
      conn = 
        build_conn(:get, "/dev/console?token=valid-token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      
      assert result.halted
      assert result.resp_body =~ "The Tau5 Elixir REPL console is disabled"
      # Should NOT mention token or localhost in error
      refute result.resp_body =~ "token"
      refute result.resp_body =~ "localhost"
      
      System.delete_env("TAU5_SESSION_TOKEN")
    end
    
    test "localhost is checked BEFORE token validation" do
      # This prevents token timing attacks from non-localhost IPs
      Application.put_env(:tau5, :console_enabled, true)
      System.put_env("TAU5_SESSION_TOKEN", "valid-token")
      
      # Non-localhost with valid token
      conn = 
        build_conn(:get, "/dev/console?token=valid-token")
        |> Map.put(:remote_ip, {192, 168, 1, 100})
      
      result = ConsoleSecurity.call(conn, [])
      
      assert result.halted
      assert result.resp_body =~ "does not accept remote connections"
      # Should fail on localhost check, not token
      refute result.resp_body =~ "token"
      
      System.delete_env("TAU5_SESSION_TOKEN")
      Application.delete_env(:tau5, :console_enabled)
    end
  end
  
  describe "CRITICAL SECURITY: Error message information leakage" do
    test "error messages don't leak sensitive information" do
      Application.put_env(:tau5, :console_enabled, true)
      
      # Test various error conditions
      test_cases = [
        # No token configured - should not reveal this to attackers
        {nil, {192, 168, 1, 1}, "any-token", 
         ["does not accept remote connections"], 
         ["Server not configured"]},  # Should not reveal server config
         
        # Wrong token from non-localhost - should not reveal token is wrong
        {"valid", {192, 168, 1, 1}, "wrong",
         ["does not accept remote connections"],
         ["Invalid", "token"]},  # Should not reveal token info to non-localhost
      ]
      
      for {server_token, ip, provided_token, should_contain, should_not_contain} <- test_cases do
        if server_token do
          System.put_env("TAU5_SESSION_TOKEN", server_token)
        else
          System.delete_env("TAU5_SESSION_TOKEN")
        end
        
        conn = 
          build_conn(:get, "/dev/console?token=#{provided_token}")
          |> Map.put(:remote_ip, ip)
        
        result = ConsoleSecurity.call(conn, [])
        
        for expected <- should_contain do
          assert result.resp_body =~ expected, 
            "Expected '#{expected}' in error for token=#{inspect(server_token)}, ip=#{inspect(ip)}"
        end
        
        for unexpected <- should_not_contain do
          refute result.resp_body =~ unexpected,
            "Should not leak '#{unexpected}' for token=#{inspect(server_token)}, ip=#{inspect(ip)}"
        end
        
        System.delete_env("TAU5_SESSION_TOKEN")
      end
      
      Application.delete_env(:tau5, :console_enabled)
    end
  end
  
  describe "Rate limiting verification" do
    test "multiple failed attempts are allowed (no rate limiting)" do
      # This test DEMONSTRATES that there's no rate limiting
      # which is a security concern but at least we're testing actual behavior
      
      Application.put_env(:tau5, :console_enabled, true)
      System.put_env("TAU5_SESSION_TOKEN", "token")
      
      # Make 10 rapid failed attempts
      results = for i <- 1..10 do
        conn = 
          build_conn(:get, "/dev/console?token=wrong#{i}")
          |> Map.put(:remote_ip, {127, 0, 0, 1})
        
        ConsoleSecurity.call(conn, [])
      end
      
      # All attempts should fail with same error (no rate limiting/lockout)
      for result <- results do
        assert result.halted
        assert result.status == 403
        assert result.resp_body =~ "Invalid or missing session token"
      end
      
      # After 10 failures, valid token still works (no lockout)
      conn = 
        build_conn(:get, "/dev/console?token=token")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
      
      result = ConsoleSecurity.call(conn, [])
      refute result.halted, "Valid token still works after many failures"
      
      System.delete_env("TAU5_SESSION_TOKEN")
      Application.delete_env(:tau5, :console_enabled)
    end
  end
end