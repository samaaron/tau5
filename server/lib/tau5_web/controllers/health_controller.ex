defmodule Tau5Web.HealthController do
  use Tau5Web, :controller

  def index(conn, _params) do
    # Minimal health check - if we can respond, Phoenix is ready to serve
    text(conn, "ok")
  end
end
