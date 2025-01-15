defmodule Tau5.Discovery.BroadcastSupervisor do
  use DynamicSupervisor
  require Logger

  def start_link(_args) do
    DynamicSupervisor.start_link(__MODULE__, :ok, name: __MODULE__)
  end

  @impl true
  def init(:ok) do
    DynamicSupervisor.init(strategy: :one_for_one)
  end

  def start_discovery_broadcaster(
        interface,
        uuid,
        metadata,
        multicast_addr,
        discovery_port
      ) do
    ack_port = Tau5.Discovery.AckReceiver.port()

    args = %{
      uuid: uuid,
      metadata: metadata,
      interface: interface,
      ack_port: ack_port,
      multicast_addr: multicast_addr,
      discovery_port: discovery_port
    }

    case DynamicSupervisor.start_child(
           __MODULE__,
           {Tau5.Discovery.Broadcaster, args}
         ) do
      {:ok, pid} ->
        Logger.info("Started discovery broadcaster for interface #{inspect(interface)}")
        {:ok, pid}

      {:error, {:already_started, _pid}} ->
        Logger.info("Discovery broadcaster already running for interface #{inspect(interface)}")
        {:ok, :already_started}

      {:error, reason} ->
        Logger.error(
          "Discovery failed to start broadcaster for interface #{inspect(interface)}: #{inspect(reason)}"
        )
    end
  end

  def stop_discovery_broadcaster(interface) do
    child = find_child_by_interface(interface)

    case child do
      pid when is_pid(pid) ->
        case DynamicSupervisor.terminate_child(__MODULE__, pid) do
          :ok ->
            Logger.info("Discovery stopped broadcaster for interface #{inspect(interface)}")
            :ok

          {:error, reason} ->
            Logger.error(
              "Discovery failed to stop broadcaster for interface #{inspect(interface)}: #{inspect(reason)}"
            )
        end

      _ ->
        Logger.info("Discovery could not find a broadcaster for interface #{inspect(interface)}")
        {:error, :not_found}
    end
  end

  defp find_child_by_interface(interface) do
    DynamicSupervisor.which_children(__MODULE__)
    |> Enum.find_value(fn
      {_, pid, _, _} when is_pid(pid) ->
        case Tau5.Discovery.Broadcaster.interface(pid) do
          ^interface -> pid
          _ -> nil
        end

      _ ->
        nil
    end) ||
      :error
  end
end
