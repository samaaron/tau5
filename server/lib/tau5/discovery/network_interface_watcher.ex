defmodule Tau5.Discovery.NetworkInterfaceWatcher do
  use GenServer
  require Logger

  @update_interval 10_000

  def start_link(args) do
    GenServer.start_link(__MODULE__, args, name: __MODULE__)
  end

  @impl true
  def init(%{uuid: uuid, metadata: metadata}) do
    state = %{interfaces: MapSet.new(), uuid: uuid, metadata: metadata}
    schedule_update()
    {:ok, state}
  end

  @impl true
  def handle_info(:update_interfaces, state) do
    state = update_interfaces(state)
    schedule_update()
    {:noreply, state}
  end

  defp schedule_update do
    Process.send_after(self(), :update_interfaces, @update_interval)
  end

  defp update_interfaces(state) do
    current_interfaces = MapSet.new(get_local_ips())

    new_interfaces = MapSet.difference(current_interfaces, state.interfaces)
    removed_interfaces = MapSet.difference(state.interfaces, current_interfaces)

    Enum.each(new_interfaces, fn interface ->
      Tau5.Discovery.BroadcastSupervisor.start_server(interface, state.uuid, state.metadata)
    end)

    Enum.each(removed_interfaces, fn interface ->
      Tau5.Discovery.BroadcastSupervisor.stop_server(interface)
    end)

    %{state | interfaces: current_interfaces}
  end

  defp get_local_ips do
    case :inet.getif() do
      {:ok, ifs} ->
        ifs
        |> Enum.map(&elem(&1, 0))
        |> Enum.reject(&(&1 == {127, 0, 0, 1}))

      {:error, _} ->
        []
    end
  end
end
