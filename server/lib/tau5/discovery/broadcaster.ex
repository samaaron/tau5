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

  @impl true
  def init(%{
        uuid: uuid,
        hostname: hostname,
        metadata: metadata,
        interface: interface,
        ack_port: ack_port,
        multicast_addr: multicast_addr,
        discovery_port: discovery_port,
        token: token
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
        {:multicast_ttl, 4}
        # {:multicast_loop, true}
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
      hostname: hostname,
      metadata: metadata,
      socket: socket,
      interface: interface,
      ack_port: ack_port,
      multicast_addr: multicast_addr,
      discovery_port: discovery_port,
      token: token
    }

    send(self(), :broadcast)
    {:ok, state}
  end

  @impl true
  def handle_info(:broadcast, state) do
    with %{
           uuid: uuid,
           hostname: hostname,
           metadata: metadata,
           socket: socket,
           interface: interface,
           ack_port: ack_port,
           multicast_addr: multicast_addr,
           discovery_port: discovery_port
         } <- state do
      message = %{
        cmd: "hello!",
        hostname: hostname,
        uuid: uuid,
        ip: Tuple.to_list(interface),
        ack_port: ack_port,
        metadata: metadata,
        token: state.token
      }

      Logger.debug("Discovery broadcasting message: #{inspect(message)}")

      :gen_udp.send(socket, multicast_addr, discovery_port, Jason.encode!(message))

      schedule_broadcast()
    end

    {:noreply, state}
  end

  def handle_info(
        {:udp, socket, src_ip, src_port, data},
        state
      ) do
    case Jason.decode(data) do
      {
        :ok,
        %{
          "cmd" => "hello!",
          "uuid" => sender_uuid,
          "hostname" => sender_hostname,
          "metadata" => sender_metadata,
          "ack_port" => sender_ack_port,
          "token" => sender_token
        }
      }
      when sender_uuid != state.uuid ->
        Logger.debug(
          "Discovery broadcaster received UDP message on interface #{inspect(state.interface)} from #{inspect(src_ip)}:#{inspect(src_port)} -----> #{data}"
        )

        Tau5.Discovery.KnownNodes.add_node(
          sender_uuid,
          state.interface,
          hostname: sender_hostname,
          metadata: sender_metadata,
          ip: src_ip
        )

        {:ok, ack} =
          Jason.encode(%{
            cmd: "ack",
            uuid: state.uuid,
            hostname: state.hostname,
            ack_port: state.ack_port,
            metadata: state.metadata,
            token: sender_token
          })

        Logger.debug(
          "Discovery sending ack to #{inspect(src_ip)}:#{sender_ack_port}, #{inspect(ack)}"
        )

        :gen_udp.send(socket, src_ip, sender_ack_port, ack)

        {:noreply, state}

      {:ok, %{"uuid" => uuid}} when uuid == state.uuid ->
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

  @impl true
  def handle_call(:get_interface, _from, state) do
    {:reply, state.interface, state}
  end

  @impl true
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
