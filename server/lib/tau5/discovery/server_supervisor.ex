defmodule Tau5.Discovery.ServerSupervisor do
  use DynamicSupervisor
  require Logger

  def start_link(_args) do
    DynamicSupervisor.start_link(__MODULE__, :ok, name: __MODULE__)
  end

  @impl true
  def init(:ok) do
    DynamicSupervisor.init(strategy: :one_for_one)
  end

  def start_server(interface, uuid, metadata) do
    spec = %{
      id: {:discovery, interface},
      start:
        {Tau5.Discovery.Server, :start_link,
         [%{uuid: uuid, metadata: metadata, interface: interface}]},
      type: :worker,
      restart: :transient
    }

    case DynamicSupervisor.start_child(__MODULE__, spec) do
      {:ok, pid} ->
        Logger.info("Started discovery server for interface #{inspect(interface)}")
        {:ok, pid}

      {:error, {:already_started, _pid}} ->
        Logger.info("Server already running for interface #{inspect(interface)}")
        {:ok, :already_started}

      {:error, reason} ->
        Logger.error(
          "Failed to start server for interface #{inspect(interface)}: #{inspect(reason)}"
        )
    end
  end

  def stop_server(interface) do
    # Find the PID of the child process with the given `child_id`
    case DynamicSupervisor.which_children(__MODULE__)
         |> Enum.find(fn {id, _pid, _type, _modules} -> id == {:discovery, interface} end) do
      {^interface, pid, _type, _modules} when is_pid(pid) ->
        case DynamicSupervisor.terminate_child(__MODULE__, pid) do
          :ok ->
            Logger.info("Stopped discovery server for interface #{inspect(interface)}")
            :ok

          {:error, reason} ->
            Logger.error(
              "Failed to stop server for interface #{inspect(interface)}: #{inspect(reason)}"
            )

            {:error, reason}
        end

      nil ->
        Logger.warning("No child found for interface #{inspect(interface)}")
        {:error, :not_found}
    end
  end
end
