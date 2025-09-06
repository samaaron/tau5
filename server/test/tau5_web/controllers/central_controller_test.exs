defmodule Tau5Web.CentralControllerTest do
  use Tau5Web.ConnCase

  describe "index" do
    test "redirects to /app when not in central mode", %{conn: conn} do
      # Default test environment is not central mode
      conn = get(conn, ~p"/")
      assert redirected_to(conn) == "/app"
    end

    test "renders central page when in central mode", %{conn: conn} do
      # Temporarily set deployment mode to central
      original_mode = Application.get_env(:tau5, :deployment_mode)
      Application.put_env(:tau5, :deployment_mode, :central)

      conn = get(conn, ~p"/")
      response = html_response(conn, 200)

      # Check for key elements of the central page
      assert response =~ "tau5.live"
      assert response =~ "canvas"
      assert response =~ "tau5-loading"

      # Restore original mode
      Application.put_env(:tau5, :deployment_mode, original_mode)
    end
  end
end
