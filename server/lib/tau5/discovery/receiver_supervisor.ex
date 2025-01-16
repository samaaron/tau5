defmodule Tau5.Discovery.ReceiverSupervisor do
  use DynamicSupervisor
  require Logger

  def start_link(_args) do
    DynamicSupervisor.start_link(__MODULE__, :ok, name: __MODULE__)
  end

  @impl true
  def init(:ok) do
    DynamicSupervisor.init(strategy: :one_for_one)
  end

  def start_receiver(interface, token) do
    case find_child_by_interface(interface) do
      :error ->
        case DynamicSupervisor.start_child(
               __MODULE__,
               {Tau5.Discovery.AckReceiver, {interface, token}}
             ) do
          {:ok, pid} ->
            Logger.info(
              "Discovery started receiver with pid #{inspect(pid)} for interface #{inspect(interface)}"
            )

            {:ok, pid}

          {:error, reason} ->
            Logger.error(
              "Discovery failed to start receiver for interface #{inspect(interface)}: #{inspect(reason)}"
            )
        end

      _ ->
        Logger.info("Discovery receiver already running for interface #{inspect(interface)}")
        {:ok, :already_started}
    end
  end

  def stop_receiver(interface) do
    child = find_child_by_interface(interface)

    case child do
      pid when is_pid(pid) ->
        case DynamicSupervisor.terminate_child(__MODULE__, pid) do
          :ok ->
            Logger.info("Discovery stopped receiver for interface #{inspect(interface)}")
            :ok

          {:error, reason} ->
            Logger.error(
              "Discovery failed to stop receiver for interface #{inspect(interface)}: #{inspect(reason)}"
            )
        end

      _ ->
        Logger.info("Discovery could not find a receiver for interface #{inspect(interface)}")
        {:error, :not_found}
    end
  end

  defp find_child_by_interface(interface) do
    DynamicSupervisor.which_children(__MODULE__)
    |> Enum.find_value(fn
      {_, pid, _, _} when is_pid(pid) ->
        case Tau5.Discovery.AckReceiver.interface(pid) do
          ^interface -> pid
          _ -> nil
        end

      _ ->
        nil
    end) ||
      :error
  end
end
