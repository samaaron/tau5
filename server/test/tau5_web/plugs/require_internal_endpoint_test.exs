defmodule Tau5Web.Plugs.RequireInternalEndpointTest do
  use Tau5Web.ConnCase
  alias Tau5Web.Plugs.RequireInternalEndpoint

  describe "call/2 with internal endpoint" do
    test "allows request from internal endpoint", %{conn: conn} do
      conn = %{conn | private: %{phoenix_endpoint: Tau5Web.Endpoint}}
      conn = %{conn | request_path: "/dev/dashboard"}

      result = RequireInternalEndpoint.call(conn, [])

      # Should not halt the connection
      refute result.halted
      assert result.status == nil
    end

    test "passes through without modification", %{conn: conn} do
      original_conn = %{conn | private: %{phoenix_endpoint: Tau5Web.Endpoint}}
      result = RequireInternalEndpoint.call(original_conn, [])

      # Connection should be unchanged except for passing through
      assert result.host == original_conn.host
      assert result.method == original_conn.method
      assert result.request_path == original_conn.request_path
    end
  end

  describe "call/2 with public endpoint" do
    test "blocks request from public endpoint", %{conn: conn} do
      conn = %{conn | private: %{phoenix_endpoint: Tau5Web.PublicEndpoint}}
      conn = %{conn | request_path: "/dev/dashboard"}

      result = RequireInternalEndpoint.call(conn, [])

      # Should halt with 404
      assert result.halted
      assert result.status == 404
      assert result.resp_body =~ "404 - Not Found"
    end

    test "returns styled 404 page", %{conn: conn} do
      conn = %{conn | private: %{phoenix_endpoint: Tau5Web.PublicEndpoint}}
      conn = %{conn | request_path: "/dev/console"}

      result = RequireInternalEndpoint.call(conn, [])

      # Check HTML content
      assert result.resp_body =~ "<!DOCTYPE html>"
      assert result.resp_body =~ "404 - Not Found"
      assert result.resp_body =~ "The requested resource does not exist"

      # Check content type
      assert Plug.Conn.get_resp_header(result, "content-type") == ["text/html; charset=utf-8"]
    end

    test "blocks various sensitive paths", %{conn: conn} do
      sensitive_paths = [
        "/dev/dashboard",
        "/dev/console",
        "/dev/mailbox",
        "/dev/anything"
      ]

      for path <- sensitive_paths do
        conn = %{conn | private: %{phoenix_endpoint: Tau5Web.PublicEndpoint}, request_path: path}

        result = RequireInternalEndpoint.call(conn, [])

        assert result.halted, "Failed to block path: #{path}"
        assert result.status == 404
      end
    end
  end

  describe "security considerations" do
    test "does not leak information about protected routes", %{conn: conn} do
      # Test both existing and non-existing protected routes
      existing_route = %{
        conn
        | private: %{phoenix_endpoint: Tau5Web.PublicEndpoint},
          request_path: "/dev/dashboard"
      }

      non_existing_route = %{
        conn
        | private: %{phoenix_endpoint: Tau5Web.PublicEndpoint},
          request_path: "/dev/nonexistent"
      }

      result1 = RequireInternalEndpoint.call(existing_route, [])
      result2 = RequireInternalEndpoint.call(non_existing_route, [])

      # Both should return identical 404 responses
      assert result1.status == result2.status
      assert result1.resp_body == result2.resp_body
    end

    test "uses 404 instead of 403 to avoid revealing route existence", %{conn: conn} do
      conn = %{
        conn
        | private: %{phoenix_endpoint: Tau5Web.PublicEndpoint},
          request_path: "/dev/console"
      }

      result = RequireInternalEndpoint.call(conn, [])

      # Should use 404, not 403
      assert result.status == 404
      refute result.status == 403
    end
  end
end
