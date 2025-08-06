defmodule Tau5Web.Router do
  use Tau5Web, :router
  
  alias Hermes.Server.Transport.StreamableHTTP

  pipeline :browser do
    plug(:accepts, ["html"])
    plug(:fetch_session)
    plug(:fetch_live_flash)
    plug(:put_root_layout, html: {Tau5Web.Layouts, :root})
    plug(:protect_from_forgery)
    plug(:put_secure_browser_headers)
  end

  pipeline :api do
    plug(:accepts, ["json"])
  end

  pipeline :console_secure do
    plug(Tau5Web.Plugs.ConsoleSecurity)
  end

  scope "/", Tau5Web do
    pipe_through(:browser)

    # Both routes exist, controller will handle the mode check
    get("/", CentralController, :index)
    live("/app", MainLive)
  end

  pipeline :sse do
    plug :accepts, ["json", "event-stream"]
  end

  scope "/tau5/mcp" do
    pipe_through(:sse)
    
    # Use Streamable HTTP transport - handles GET, POST, DELETE at the same endpoint
    forward("/", StreamableHTTP.Plug, server: Tau5MCP.Server)
  end

  if Application.compile_env(:tau5, :dev_routes) do
    import Phoenix.LiveDashboard.Router

    scope "/dev" do
      pipe_through(:browser)
      
      live_dashboard("/dashboard", metrics: Tau5Web.Telemetry)
      forward("/mailbox", Plug.Swoosh.MailboxPreview)
    end

    if Application.compile_env(:tau5, :console_enabled, false) do
      scope "/dev" do
        pipe_through([:browser, :console_secure])
        live("/console", Tau5Web.ConsoleLive, :console)
      end
    end
  end
end
