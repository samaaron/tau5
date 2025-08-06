defmodule Tau5Web.PageControllerTest do
  use Tau5Web.ConnCase

  test "GET / redirects to /app in non-central mode", %{conn: conn} do
    conn = get(conn, ~p"/")
    assert redirected_to(conn) == "/app"
  end

  test "GET /app loads the main LiveView", %{conn: conn} do
    conn = get(conn, ~p"/app")
    assert html_response(conn, 200) =~ "phx-"
  end
end
