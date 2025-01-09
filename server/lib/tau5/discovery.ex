defmodule Tau5.Discovery do
  use GenServer
  require Logger

  @multicast_addr {224, 0, 0, 251}
  @discovery_port 50000
  @interval 5_000
  @sol_socket 0xFFFF
  @so_reuseport 0x0200
  @so_reuseaddr 0x0004

  def known_nodes() do
    GenServer.call(__MODULE__, :known_nodes)
  end

  def start_link(args) do
    GenServer.start_link(__MODULE__, args, name: __MODULE__)
  end

  def init(%{uuid: uuid, metadata: metadata}) do
    {:ok, socket} =
      :gen_udp.open(
        @discovery_port,
        [
          :binary,
          {:active, true},
          {:reuseaddr, true},
          {:reuseport, true},
          {:multicast_if, {0, 0, 0, 0}},
          {:multicast_loop, true}
        ] ++ [{:raw, @sol_socket, @so_reuseaddr, <<1::native-32>>}]
        # currently hardcoded for Windows...
      )

    Logger.info("Socket opened on port #{@discovery_port}")

    :ok = :inet.setopts(socket, [{:add_membership, {@multicast_addr, {0, 0, 0, 0}}}])
    Logger.info("Joined multicast group #{inspect(@multicast_addr)}")

    state = %{
      uuid: uuid,
      metadata: metadata,
      socket: socket,
      known_nodes: %{}
    }

    schedule_broadcast()

    {:ok, state}
  end

  def handle_info(:broadcast, %{uuid: uuid, metadata: metadata, socket: socket} = state) do
    ips = get_local_ips()

    Enum.each(ips, fn ip ->
      message = %{
        uuid: uuid,
        ip: Tuple.to_list(ip),
        metadata: metadata
      }

      Logger.info("Broadcasting message: #{inspect(message)}")

      :gen_udp.send(socket, @multicast_addr, @discovery_port, Jason.encode!(message))
    end)

    schedule_broadcast()
    {:noreply, state}
  end

  def handle_info({:udp, _socket, src_ip, src_port, data}, %{known_nodes: known_nodes} = state) do
    Logger.info("Received UDP message from: #{inspect(src_ip)}:#{src_port} #{inspect(data)}")

    case Jason.decode(data) do
      {:ok, %{"uuid" => uuid, "ip" => ip, "metadata" => metadata}} ->
        ip_tuple = List.to_tuple(ip)

        if uuid != state.uuid do
          updated_nodes =
            Map.put(known_nodes, uuid, %{
              ip: ip_tuple,
              metadata: metadata,
              last_seen: :os.system_time(:millisecond)
            })

          Logger.info("Updated known nodes: #{inspect(updated_nodes)}")
          {:noreply, %{state | known_nodes: updated_nodes}}
        else
          {:noreply, state}
        end

      error ->
        Logger.error("Failed to decode message: #{inspect(error)}")
        {:noreply, state}
    end
  end

  def handle_call(:known_nodes, _from, state) do
    {:reply, state.known_nodes, state}
  end

  defp schedule_broadcast do
    Process.send_after(self(), :broadcast, @interval)
  end

  defp get_local_ips do
    {:ok, ifs} = :inet.getif()

    ifs
    |> Enum.map(&elem(&1, 0))
    |> Enum.reject(&(&1 == {127, 0, 0, 1}))
  end
end
