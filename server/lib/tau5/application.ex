defmodule Tau5.Application do
  # See https://hexdocs.pm/elixir/Application.html
  # for more information on OTP Applications
  @moduledoc false

  use Application
  require Logger

  @impl true
  def start(_type, _args) do
    uuid = UUID.uuid4()
    Logger.info("Node #{uuid} booting....")

    children = [
      Tau5Web.Telemetry,
      {Phoenix.PubSub, name: Tau5.PubSub},
      # Start the Finch HTTP client for sending emails
      {Finch, name: Tau5.Finch},
      # Start a worker by calling: Tau5.Worker.start_link(arg)
      # {Tau5.Worker, arg},
      # Start to serve requests, typically the last entry
      Tau5Web.Endpoint,
      {Tau5.Discovery, %{uuid: uuid, metadata: %{name: "foo", version: "1.0.0"}}}
    ]

    # See https://hexdocs.pm/elixir/Supervisor.html
    # for other strategies and supported options
    opts = [strategy: :one_for_one, name: Tau5.Supervisor]
    Supervisor.start_link(children, opts)
  end

  # Tell Phoenix to update the endpoint configuration
  # whenever the application is updated.
  @impl true
  def config_change(changed, _new, removed) do
    Tau5Web.Endpoint.config_change(changed, removed)
    :ok
  end
end
