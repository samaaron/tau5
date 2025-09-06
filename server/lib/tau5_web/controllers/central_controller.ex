defmodule Tau5Web.CentralController do
  use Tau5Web, :controller

  def index(conn, _params) do
    case Application.get_env(:tau5, :deployment_mode) do
      :central ->
        render(conn, :index,
          vertex_shader_path: ~p"/shaders/tau5-loading.vert",
          fragment_shader_path: ~p"/shaders/tau5-loading.frag",
          font_path: ~p"/fonts/CascadiaCodePL.woff2",
          logo_path: ~p"/images/tau5-bw-hirez.png",
          layout: false
        )

      _ ->
        redirect(conn, to: "/app")
    end
  end
end
