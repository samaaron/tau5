defmodule Tau5.Discovery.Broadcaster do
  use GenServer, restart: :transient
  require Logger

  # The multicast address is the same as the one used by Ableton's Link protocol
  # The port is one higher than that used by Link.
  @multicast_addr {224, 76, 78, 75}
  @discovery_port 20809
  @broadcast_interval 10_000
  @max_node_age 30_000

  @sol_socket 0xFFFF
  # @so_reuseport 0x0200
  @so_reuseaddr 0x0004

  def interface(pid) do
    GenServer.call(pid, :get_interface)
  end

  def start_link(args) do
    GenServer.start_link(__MODULE__, args)
  end

  def init(%{uuid: uuid, metadata: metadata, interface: interface, ack_port: ack_port}) do
    Logger.error("broadcaster init")
    # Create UDP socket for multicast
    socket_options =
      [
        :binary,
        {:active, true},
        {:reuseaddr, true},
        {:reuseport, true},
        {:ip, interface},
        {:multicast_if, interface},
        {:multicast_ttl, 32},
        {:multicast_loop, true}
      ] ++ [{:raw, @sol_socket, @so_reuseaddr, <<1::native-32>>}]

    {:ok, socket} = :gen_udp.open(@discovery_port, socket_options)

    Logger.debug("Socket opened on port #{@discovery_port} for interface #{inspect(interface)}")

    :ok = :inet.setopts(socket, [{:add_membership, {@multicast_addr, interface}}])

    Logger.debug(
      "Joined multicast group #{inspect(@multicast_addr)} on interface #{inspect(interface)}"
    )

    state = %{
      uuid: uuid,
      metadata: metadata,
      socket: socket,
      interface: interface,
      ack_port: ack_port
    }

    schedule_broadcast()

    {:ok, state}
  end

  def handle_info(:broadcast, state) do
    with %{
           uuid: uuid,
           metadata: metadata,
           socket: socket,
           interface: interface,
           ack_port: ack_port
         } <- state do
      message = %{
        cmd: "hello",
        uuid: uuid,
        ip: Tuple.to_list(interface),
        ack_port: ack_port,
        metadata: metadata
      }

      Logger.debug("Broadcasting message: #{inspect(message)}")

      :gen_udp.send(socket, @multicast_addr, @discovery_port, Jason.encode!(message))

      schedule_broadcast()
      {:noreply, state}
    end
  end

  def handle_info(
        {:udp, socket, src_ip, src_port, data},
        %{uuid: own_uuid, ack_port: own_ack_port} = state
      ) do
    Logger.debug("Broadcaster received UDP message from: #{inspect(src_ip)}:#{src_port} #{data}")

    case Jason.decode(data) do
      {:ok,
       %{
         "uuid" => sender_uuid,
         "metadata" => sender_metadata,
         "ack_port" => sender_ack_port
       }}
      when sender_uuid != own_uuid ->
        Tau5.Discovery.KnownNodes.add_node(
          sender_uuid,
          uuid: sender_uuid,
          metadata: sender_metadata,
          ip: src_ip
        )

        {:ok, ack} =
          Jason.encode(%{
            cmd: "ack",
            uuid: own_uuid,
            ip: Tuple.to_list(state.interface),
            ack_port: own_ack_port,
            metadata: state.metadata
          })

        Logger.debug("sending ack to #{inspect(src_ip)}:#{sender_ack_port}, #{inspect(ack)}")

        :gen_udp.send(socket, src_ip, sender_ack_port, ack)

        {:noreply, state}

      {:ok, %{"uuid" => uuid}} when uuid == own_uuid ->
        # Logger.debug("Ignored own message")
        {:noreply, state}

      {:ok, msg} ->
        Logger.error("Broadcaster received unexpected JSON -----> #{inspect(msg)}")
        {:noreply, state}

      {:error, info} ->
        Logger.error("Broadcaster failed to decode message: #{inspect(info)}")
        {:noreply, state}

      error ->
        Logger.error(
          "Broadcaster encountered a critical error decoding incoming UDP message as JSON: #{inspect(error)}"
        )

        {:noreply, state}
    end
  end

  def handle_call(:get_interface, _from, state) do
    {:reply, state.interface, state}
  end

  def terminate(_reason, %{socket: socket, interface: interface}) do
    :ok = :inet.setopts(socket, [{:drop_membership, {@multicast_addr, interface}}])
    :gen_udp.close(socket)

    Logger.debug(
      "Broadcaster for interface #{inspect(interface)} has closed its socket and left multicast group"
    )

    :ok
  end

  defp schedule_broadcast do
    Process.send_after(self(), :broadcast, @broadcast_interval)
  end
end
