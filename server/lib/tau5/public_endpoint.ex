defmodule Tau5.PublicEndpoint do
  @moduledoc """
  GenServer that manages the enabled/disabled state of the public endpoint.
  The endpoint itself always runs, but this controls whether remote connections are accepted.
  """
  use GenServer
  require Logger

  @name __MODULE__

  # Client API

  def start_link(opts) do
    initial_state = Keyword.get(opts, :enabled, false)
    GenServer.start_link(__MODULE__, initial_state, name: @name)
  end

  @doc """
  Enable remote access to the public endpoint.
  """
  def enable do
    GenServer.call(@name, :enable)
  end

  @doc """
  Disable remote access to the public endpoint.
  """
  def disable do
    GenServer.call(@name, :disable)
  end

  @doc """
  Toggle the current state of remote access.
  """
  def toggle do
    GenServer.call(@name, :toggle)
  end

  @doc """
  Get the current status of the public endpoint.
  Returns {:enabled, port: port} or {:disabled, port: port}
  """
  def status do
    GenServer.call(@name, :status)
  end

  @doc """
  Check if remote access is currently enabled.
  Used by the access control plug.
  """
  def enabled? do
    GenServer.call(@name, :enabled?)
  end

  @doc """
  Get the port the public endpoint is listening on.
  Returns nil if the endpoint is not running.
  """
  def port do
    case get_endpoint_info() do
      {:ok, port} -> port
      _ -> nil
    end
  end

  @impl true
  def init(initial_enabled) do
    Logger.info(
      "PublicEndpoint starting with remote access #{if initial_enabled, do: "enabled", else: "disabled"}"
    )

    {:ok, %{enabled: initial_enabled}}
  end

  @impl true
  def handle_call(:enable, _from, state) do
    Logger.info("PublicEndpoint: Enabling remote access")
    new_state = %{state | enabled: true}
    {:reply, :ok, new_state}
  end

  @impl true
  def handle_call(:disable, _from, state) do
    Logger.info("PublicEndpoint: Disabling remote access")
    new_state = %{state | enabled: false}
    {:reply, :ok, new_state}
  end

  @impl true
  def handle_call(:toggle, _from, state) do
    new_enabled = not state.enabled

    Logger.info(
      "PublicEndpoint: Toggling remote access to #{if new_enabled, do: "enabled", else: "disabled"}"
    )

    new_state = %{state | enabled: new_enabled}
    {:reply, {:ok, new_enabled}, new_state}
  end

  @impl true
  def handle_call(:status, _from, state) do
    status =
      if state.enabled do
        case get_endpoint_info() do
          {:ok, port} -> {:enabled, port: port}
          _ -> {:enabled, port: nil}
        end
      else
        case get_endpoint_info() do
          {:ok, port} -> {:disabled, port: port}
          _ -> {:disabled, port: nil}
        end
      end

    {:reply, status, state}
  end

  @impl true
  def handle_call(:enabled?, _from, state) do
    {:reply, state.enabled, state}
  end

  defp get_endpoint_info do
    config = Application.get_env(:tau5, Tau5Web.PublicEndpoint, [])
    http_config = Keyword.get(config, :http, [])

    case Keyword.get(http_config, :port) do
      nil -> {:error, :no_port}
      port -> {:ok, port}
    end
  end
end
