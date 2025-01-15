defmodule Tau5.Discovery.Broadcaster do
  use GenServer, restart: :transient
  require Logger

  @broadcast_interval 15_000

  def interface(pid) do
    GenServer.call(pid, :get_interface)
  end

  def start_link(args) do
    GenServer.start_link(__MODULE__, args)
  end

  def init(%{
        uuid: uuid,
        metadata: metadata,
        interface: interface,
        ack_port: ack_port,
        multicast_addr: multicast_addr,
        discovery_port: discovery_port
      }) do
    # Create UDP socket for multicast
    socket_options =
      [
        :binary,
        {:active, true},
        {:reuseaddr, true},
        {:reuseport, true},
        {:ip, interface},
        {:multicast_if, interface},
        {:multicast_ttl, 4},
        {:multicast_loop, true}
      ]

    {:ok, socket} = :gen_udp.open(discovery_port, socket_options)

    Logger.debug(
      "Discovery broadcast socket opened on port #{discovery_port} for interface #{inspect(interface)}"
    )

    :ok = :inet.setopts(socket, [{:add_membership, {multicast_addr, interface}}])

    Logger.debug(
      "Discovery joined multicast group #{inspect(multicast_addr)} on interface #{inspect(interface)}"
    )

    state = %{
      uuid: uuid,
      metadata: metadata,
      socket: socket,
      interface: interface,
      ack_port: ack_port,
      multicast_addr: multicast_addr,
      discovery_port: discovery_port
    }

    send(self(), :broadcast)
    schedule_broadcast()

    {:ok, state}
  end

  def handle_info(:broadcast, state) do
    with %{
           uuid: uuid,
           metadata: metadata,
           socket: socket,
           interface: interface,
           ack_port: ack_port,
           multicast_addr: multicast_addr,
           discovery_port: discovery_port
         } <- state do
      message = %{
        cmd: "hello",
        uuid: uuid,
        ip: Tuple.to_list(interface),
        ack_port: ack_port,
        metadata: metadata
      }

      Logger.debug("Discovery broadcasting message: #{inspect(message)}")

      :gen_udp.send(socket, multicast_addr, discovery_port, Jason.encode!(message))

      schedule_broadcast()
      {:noreply, state}
    end
  end

  def handle_info(
        {:udp, socket, src_ip, src_port, data},
        %{uuid: own_uuid, ack_port: own_ack_port} = state
      ) do
    Logger.debug(
      "Discovery broadcaster received UDP message from: #{inspect(src_ip)}:#{src_port} #{data}"
    )

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
            ack_port: own_ack_port,
            metadata: state.metadata
          })

        Logger.debug(
          "Discovery sending ack to #{inspect(src_ip)}:#{sender_ack_port}, #{inspect(ack)}"
        )

        :gen_udp.send(socket, src_ip, sender_ack_port, ack)

        {:noreply, state}

      {:ok, %{"uuid" => uuid}} when uuid == own_uuid ->
        # Logger.debug("Ignored own message")
        {:noreply, state}

      {:ok, msg} ->
        Logger.error("Discovery broadcaster received unexpected JSON -----> #{inspect(msg)}")
        {:noreply, state}

      {:error, info} ->
        Logger.error("Discovery broadcaster failed to decode message: #{inspect(info)}")
        {:noreply, state}

      error ->
        Logger.error(
          "Discovery broadcaster encountered a critical error decoding incoming UDP message as JSON: #{inspect(error)}"
        )

        {:noreply, state}
    end
  end

  def handle_call(:get_interface, _from, state) do
    {:reply, state.interface, state}
  end

  def terminate(_reason, %{socket: socket, interface: interface, multicast_addr: multicast_addr}) do
    :ok = :inet.setopts(socket, [{:drop_membership, {multicast_addr, interface}}])
    :gen_udp.close(socket)

    Logger.debug(
      "Discovery broadcaster for interface #{inspect(interface)} has closed its socket and left multicast group"
    )

    :ok
  end

  defp schedule_broadcast() do
    Process.send_after(self(), :broadcast, @broadcast_interval)
  end
end
