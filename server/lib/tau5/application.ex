defmodule Tau5.Application do
  use Application
  require Logger

  @impl true
  def start(_type, _args) do
    http_port = Application.get_env(:tau5, Tau5Web.Endpoint)[:http][:port]

    children = [
      Tau5.ConfigRepo,
      Tau5.ConfigRepoMigrator,
      Tau5Web.Telemetry,
      {Phoenix.PubSub, name: Tau5.PubSub},
      {Finch, name: Tau5.Finch},
      Tau5Web.Endpoint,
      Tau5.Heartbeat,
      Tau5.Link,
      Tau5.MIDI,
      {Tau5.Discovery, %{http_port: http_port}}
    ]

    opts = [strategy: :one_for_one, name: Tau5.Supervisor]
    
    case Supervisor.start_link(children, opts) do
      {:ok, pid} ->
        # Log unique marker that GUI can wait for
        Logger.info("[TAU5_OTP_READY] OTP supervision tree is up and running")
        {:ok, pid}
      
      error ->
        error
    end
  end

  @impl true
  def config_change(changed, _new, removed) do
    Tau5Web.Endpoint.config_change(changed, removed)
    :ok
  end
end
