defmodule Tau5Web.CentralController do
  use Tau5Web, :controller

  def index(conn, _params) do
    case Application.get_env(:tau5, :deployment_mode) do
      :central ->
        render(conn, :index, layout: false)
      _ ->
        redirect(conn, to: "/app")
    end
  end
end