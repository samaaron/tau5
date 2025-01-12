defmodule Tau5.Discovery.Server do
  use GenServer
  require Logger

  # The multicast address is the same as the one used by Ableton's Link protocol
  # The port is one higher than that used by Link.
  @multicast_addr {224, 76, 78, 75}
  @discovery_port 20809
  @broadcast_interval 10_000
  @cleanup_interval 10_000
  @max_node_age 30_000

  def known_nodes(pid) do
    GenServer.call(pid, :known_nodes)
  end

  def start_link(args) do
    GenServer.start_link(__MODULE__, args)
  end

  def init(%{uuid: uuid, metadata: metadata, interface: interface}) do
    # Create UDP socket for multicast
    socket_options = [
      :binary,
      {:active, true},
      {:reuseaddr, true},
      {:reuseport, true},
      {:multicast_if, interface},
      {:multicast_ttl, 32},
      {:multicast_loop, false}
    ]

    {:ok, socket} = :gen_udp.open(@discovery_port, socket_options)

    Logger.info("Socket opened on port #{@discovery_port} for interface #{inspect(interface)}")

    :ok = :inet.setopts(socket, [{:add_membership, {@multicast_addr, interface}}])

    Logger.info(
      "Joined multicast group #{inspect(@multicast_addr)} on interface #{inspect(interface)}"
    )

    state = %{
      uuid: uuid,
      metadata: metadata,
      socket: socket,
      interface: interface,
      known_nodes: %{}
    }

    schedule_broadcast()
    schedule_cleanup()

    {:ok, state}
  end

  def handle_info(
        :broadcast,
        %{uuid: uuid, metadata: metadata, socket: socket, interface: interface} = state
      ) do
    message = %{
      uuid: uuid,
      ip: Tuple.to_list(interface),
      metadata: metadata
    }

    Logger.info("Broadcasting message: #{inspect(message)}")

    :gen_udp.send(socket, @multicast_addr, @discovery_port, Jason.encode!(message))

    schedule_broadcast()
    {:noreply, state}
  end

  def handle_info(:cleanup, state) do
    current_time = :os.system_time(:millisecond)

    new_known_nodes =
      state.known_nodes
      |> Enum.filter(fn {_uuid, %{last_seen: last_seen}} ->
        current_time - last_seen <= @max_node_age
      end)
      |> Enum.into(%{})

    removed_nodes = Map.keys(state.known_nodes) -- Map.keys(new_known_nodes)

    if(Enum.count(removed_nodes) > 0) do
      Logger.info("Removing stale nodes: #{inspect(removed_nodes)}")
    end

    schedule_cleanup()
    {:noreply, %{state | known_nodes: new_known_nodes}}
  end

  def handle_info(
        {:udp, _socket, src_ip, src_port, data},
        %{uuid: own_uuid, known_nodes: known_nodes} = state
      ) do
    Logger.info("Received UDP message from: #{inspect(src_ip)}:#{src_port} #{inspect(data)}")

    case Jason.decode(data) do
      {:ok, %{"uuid" => uuid, "ip" => ip, "metadata" => metadata}} when uuid != own_uuid ->
        ip_tuple = List.to_tuple(ip)

        updated_nodes =
          Map.put(known_nodes, uuid, %{
            ip: ip_tuple,
            metadata: metadata,
            last_seen: :os.system_time(:millisecond)
          })

        Logger.info("Updated known nodes: #{inspect(updated_nodes)}")
        {:noreply, %{state | known_nodes: updated_nodes}}

      {:ok, _} ->
        # Logger.info("Ignored own message")
        {:noreply, state}

      error ->
        Logger.error("Failed to decode message: #{inspect(error)}")
        {:noreply, state}
    end
  end

  def handle_call(:known_nodes, _from, state) do
    {:reply, state.known_nodes, state}
  end

  def terminate(_reason, %{socket: socket, interface: interface}) do
    :ok = :inet.setopts(socket, [{:drop_membership, {@multicast_addr, interface}}])
    :gen_udp.close(socket)
    Logger.info("Socket closed and multicast group left for interface #{inspect(interface)}")
    :ok
  end

  defp schedule_broadcast do
    Process.send_after(self(), :broadcast, @broadcast_interval)
  end

  defp schedule_cleanup do
    Process.send_after(self(), :cleanup, @cleanup_interval)
  end
end
