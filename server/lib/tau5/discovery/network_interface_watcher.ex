defmodule Tau5.Discovery.NetworkInterfaceWatcher do
  use GenServer
  require Logger

  @interface_update_interval 10_000

  def start_link(args) do
    GenServer.start_link(__MODULE__, args, name: __MODULE__)
  end

  @impl true
  def init(%{
        uuid: uuid,
        hostname: hostname,
        metadata: metadata,
        multicast_addr: multicast_addr,
        discovery_port: discovery_port
      }) do
    state = %{
      uuid: uuid,
      hostname: hostname,
      interfaces: MapSet.new(),
      metadata: metadata,
      multicast_addr: multicast_addr,
      discovery_port: discovery_port
    }

    send(self(), :update_interfaces)
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
    Process.send_after(self(), :update_interfaces, @interface_update_interval)
  end

  defp update_interfaces(state) do
    current_interfaces = MapSet.new(get_local_ips())

    new_interfaces = MapSet.difference(current_interfaces, state.interfaces)
    removed_interfaces = MapSet.difference(state.interfaces, current_interfaces)

    Enum.each(new_interfaces, fn interface ->
      token = UUID.uuid4()
      {:ok, pid} = Tau5.Discovery.ReceiverSupervisor.start_receiver(interface, token)
      ack_port = Tau5.Discovery.AckReceiver.port(pid)

      Tau5.Discovery.BroadcastSupervisor.start_discovery_broadcaster(
        interface,
        state.uuid,
        state.hostname,
        state.metadata,
        state.multicast_addr,
        state.discovery_port,
        ack_port,
        token
      )
    end)

    Enum.each(removed_interfaces, fn interface ->
      Tau5.Discovery.BroadcastSupervisor.stop_discovery_broadcaster(interface)
      Tau5.Discovery.ReceiverSupervisor.stop_receiver(interface)
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
