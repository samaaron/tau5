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
    http_port = Application.get_env(:tau5, Tau5Web.Endpoint)[:http][:port]

    midi_enabled = Application.get_env(:tau5, :midi_enabled, false)
    link_enabled = Application.get_env(:tau5, :link_enabled, false)
    discovery_enabled = Application.get_env(:tau5, :discovery_enabled, false)

    if midi_enabled do
      Logger.info("Initialising MIDI native interface")
      :sp_midi.init()
    else
      Logger.info("Starting without MIDI native interface")
    end

    if link_enabled do
      Logger.info("Initialising Link native interface")
      :sp_link.init()
    else
      Logger.info("Starting without Link native interface")
    end

    if discovery_enabled do
      Logger.info("Initialising Discovery native interface")
      :tau5_discovery.init()
    else
      Logger.info("Starting without Discovery native interface")
    end
    children =
      [
        Tau5Web.Telemetry,
        {Phoenix.PubSub, name: Tau5.PubSub},
        {Finch, name: Tau5.Finch},
        Tau5Web.Endpoint,
        Tau5.Link,
        {Tau5.Discovery, %{node_uuid: uuid}}
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
