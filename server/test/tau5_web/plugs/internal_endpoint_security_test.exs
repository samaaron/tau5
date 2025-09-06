defmodule Tau5Web.Plugs.InternalEndpointSecurityTest do
  use Tau5Web.ConnCase
  alias Tau5Web.Plugs.InternalEndpointSecurity

  # Helper to build test connections with proper session setup
  defp build_test_conn(path \\ "/") do
    build_conn(:get, path)
    |> Plug.Test.init_test_session(%{})
    |> fetch_query_params()
  end

  describe "CRITICAL: IP spoofing and localhost bypass attempts" do
    setup do
      # Use the token from test config
      :ok
    end

    test "blocks localhost bypass attempts with various IP formats" do
      # These should ALL be blocked - only 127.0.0.x is allowed
      bypass_attempts = [
        # Ubuntu localhost alias - should be blocked
        {127, 0, 1, 1},
        # Still in 127.0.0.0/8 - should be blocked
        {127, 1, 1, 1},
        # Broadcast - should be blocked
        {127, 255, 255, 255},
        # All interfaces - should be blocked
        {0, 0, 0, 0},
        # ::2 - should be blocked
        {0, 0, 0, 0, 0, 0, 0, 2},
        # Private network - should be blocked
        {10, 0, 0, 1},
        # Private network - should be blocked
        {192, 168, 1, 1}
      ]

      for ip <- bypass_attempts do
        conn =
          build_test_conn("/?token=test-token-for-tests")
          |> Map.put(:remote_ip, ip)

        result = InternalEndpointSecurity.call(conn, [])

        # All of these should be blocked
        assert result.halted, "IP #{inspect(ip)} should be blocked"
        assert result.status == 403
      end
    end

    test "only allows true localhost IPs" do
      # Only these should be allowed
      allowed_ips = [
        # Standard IPv4 localhost
        {127, 0, 0, 1},
        # Also valid in 127.0.0.x range
        {127, 0, 0, 2},
        # IPv6 localhost
        {0, 0, 0, 0, 0, 0, 0, 1},
        # IPv4-mapped IPv6 localhost
        {0, 0, 0, 0, 0, 65535, 32512, 1}
      ]

      for ip <- allowed_ips do
        conn =
          build_test_conn("/?token=test-token-for-tests")
          |> Map.put(:remote_ip, ip)

        result = InternalEndpointSecurity.call(conn, [])
        refute result.halted, "IP #{inspect(ip)} should be allowed as localhost"
      end
    end

    test "blocks X-Forwarded-For header manipulation" do
      # Attacker tries to use proxy headers to bypass localhost check
      conn =
        build_test_conn("/?token=test-token-for-tests")
        |> put_req_header("x-forwarded-for", "127.0.0.1")
        |> Map.put(:remote_ip, {192, 168, 1, 100})

      result = InternalEndpointSecurity.call(conn, [])

      assert result.halted, "X-Forwarded-For bypass should be blocked"
      assert result.status == 403
      assert result.resp_body =~ "only accessible from localhost"
    end

    test "blocks X-Real-IP header manipulation" do
      conn =
        build_test_conn("/?token=test-token-for-tests")
        |> put_req_header("x-real-ip", "127.0.0.1")
        |> Map.put(:remote_ip, {10, 0, 0, 1})

      result = InternalEndpointSecurity.call(conn, [])

      assert result.halted, "X-Real-IP bypass should be blocked"
      assert result.status == 403
    end
  end

  describe "CRITICAL: Token timing attacks and validation" do
    setup do
      # Store original and set test token
      original = Application.get_env(:tau5, :session_token)
      Application.put_env(:tau5, :session_token, "aaaaaaaa")

      on_exit(fn ->
        if original do
          Application.put_env(:tau5, :session_token, original)
        else
          Application.delete_env(:tau5, :session_token)
        end
      end)

      :ok
    end

    test "token comparison is vulnerable to timing attacks" do
      # SECURITY WARNING: Current implementation uses == which is timing-vulnerable
      # This test documents the vulnerability for future fixing

      # These tokens have increasing similarity to "aaaaaaaa"
      # In a timing attack, "aaaaaaab" would theoretically take longer to reject
      tokens = [
        "bbbbbbbb",
        "abbbbbbb",
        "aabbbbbb",
        "aaabbbbb",
        "aaaabbbb",
        "aaaaabbb",
        "aaaaaabb",
        "aaaaaaab"
      ]

      for token <- tokens do
        conn =
          build_test_conn("/?token=#{token}")
          |> Map.put(:remote_ip, {127, 0, 0, 1})

        result = InternalEndpointSecurity.call(conn, [])

        assert result.halted, "Wrong token '#{token}' should be blocked"
        assert result.status == 403
        assert result.resp_body =~ "Invalid or missing app token"
      end
    end

    test "blocks empty token attempts" do
      empty_attempts = [
        # Empty query param
        "",
        # Empty value
        "token=",
        # No token at all
        nil
      ]

      for token <- empty_attempts do
        query = if token, do: "/?token=#{token}", else: "/"

        conn =
          build_test_conn(query)
          |> Map.put(:remote_ip, {127, 0, 0, 1})

        result = InternalEndpointSecurity.call(conn, [])

        assert result.halted, "Empty token #{inspect(token)} should be blocked"
        assert result.status == 403
      end
    end

    test "token in header works correctly" do
      conn =
        build_test_conn("/")
        |> put_req_header("x-tau5-token", "aaaaaaaa")
        |> Map.put(:remote_ip, {127, 0, 0, 1})

      result = InternalEndpointSecurity.call(conn, [])
      refute result.halted, "Valid token in header should work"
    end

    test "query param takes precedence over header" do
      conn =
        build_test_conn("/?token=wrong")
        |> put_req_header("x-tau5-token", "aaaaaaaa")
        |> Map.put(:remote_ip, {127, 0, 0, 1})

      result = InternalEndpointSecurity.call(conn, [])

      # Query param should win, even if wrong
      assert result.halted, "Query param should override header"
    end
  end

  describe "Session token persistence" do
    setup do
      # Ensure we're using the expected token
      original = Application.get_env(:tau5, :session_token)
      Application.put_env(:tau5, :session_token, "test-token-for-tests")

      on_exit(fn ->
        if original do
          Application.put_env(:tau5, :session_token, original)
        else
          Application.delete_env(:tau5, :session_token)
        end
      end)

      :ok
    end

    test "stores valid token in session for future requests" do
      # First request with token
      conn =
        build_test_conn("/?token=test-token-for-tests")
        |> Map.put(:remote_ip, {127, 0, 0, 1})

      result = InternalEndpointSecurity.call(conn, [])

      refute result.halted
      assert Plug.Conn.get_session(result, "app_token") == "test-token-for-tests"
    end

    test "accepts request with valid session token" do
      # Request with session but no query token
      conn =
        build_test_conn("/")
        |> Map.put(:remote_ip, {127, 0, 0, 1})
        |> Plug.Conn.put_session("app_token", "test-token-for-tests")

      result = InternalEndpointSecurity.call(conn, [])

      refute result.halted, "Valid session token should allow access"
    end
  end

  describe "Security order of operations" do
    setup do
      # Use test config token
      :ok
    end

    test "checks localhost BEFORE token validation" do
      # Remote IP with correct token should still be blocked
      conn =
        build_test_conn("/?token=test-token-for-tests")
        |> Map.put(:remote_ip, {192, 168, 1, 1})

      result = InternalEndpointSecurity.call(conn, [])

      assert result.halted
      assert result.status == 403
      # Should mention localhost, not token
      assert result.resp_body =~ "only accessible from localhost"
      refute result.resp_body =~ "token"
    end
  end

  describe "Error message information leakage" do
    test "doesn't reveal if token exists when accessed remotely" do
      # Use test config token

      conn =
        build_test_conn("/?token=wrong")
        |> Map.put(:remote_ip, {192, 168, 1, 1})

      result = InternalEndpointSecurity.call(conn, [])

      # Should not mention token at all for remote access
      assert result.resp_body =~ "only accessible from localhost"
      refute result.resp_body =~ "token"
      refute result.resp_body =~ "invalid"
    end

    test "error messages are consistent for missing vs wrong token" do
      # Use test config token

      conn_missing =
        build_test_conn("/")
        |> Map.put(:remote_ip, {127, 0, 0, 1})

      conn_wrong =
        build_test_conn("/?token=wrong")
        |> Map.put(:remote_ip, {127, 0, 0, 1})

      result_missing = InternalEndpointSecurity.call(conn_missing, [])
      result_wrong = InternalEndpointSecurity.call(conn_wrong, [])

      # Both should have identical error messages
      assert result_missing.resp_body == result_wrong.resp_body
    end
  end
end
