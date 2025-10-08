defmodule Tau5Web.Router do
  use Tau5Web, :router

  pipeline :browser do
    plug(:accepts, ["html"])
    plug(:fetch_session)
    plug(:fetch_live_flash)
    plug(:put_root_layout, html: {Tau5Web.Layouts, :root})
    plug(:protect_from_forgery)
    plug(:put_secure_browser_headers)
    plug(Tau5Web.Plugs.AccessTier)
  end

  pipeline :api do
    plug(:accepts, ["json"])
  end

  pipeline :console_secure do
    plug(Tau5Web.Plugs.ConsoleSecurity)
  end

  pipeline :require_internal_endpoint do
    plug(Tau5Web.Plugs.RequireInternalEndpoint)
  end

  scope "/", Tau5Web do
    pipe_through(:browser)

    # Both routes exist, controller will handle the mode check
    get("/", CentralController, :index)

    # Health check endpoint (kept for backwards compatibility)
    get("/health", HealthController, :index)

    live_session :default,
      on_mount: Tau5Web.AccessTierHook do
      live("/app", MainLive)
    end
  end

  pipeline :sse do
    plug :accepts, ["json", "event-stream"]
  end

  # Dev routes should only be accessible from the internal endpoint
  # We check this at runtime to ensure they're never exposed on the public endpoint
  if Application.compile_env(:tau5, :dev_routes) do
    import Phoenix.LiveDashboard.Router

    scope "/dev" do
      pipe_through(:browser)
      pipe_through(:require_internal_endpoint)

      live_dashboard("/dashboard", metrics: Tau5Web.Telemetry)
      forward("/mailbox", Plug.Swoosh.MailboxPreview)

      # Console route - additional security via ConsoleSecurity plug
      pipe_through(:console_secure)
      live("/console", Tau5Web.ConsoleLive, :console)
    end
  end
end
