defmodule Tau5.Discovery.AckReceiver do
  use GenServer, restart: :transient
  require Logger

  def start_link([]) do
    GenServer.start_link(__MODULE__, [], name: __MODULE__)
  end

  def init([]) do
    init(0)
  end

  def init(port) do
    socket_options = [
      :binary,
      {:active, true}
    ]

    {:ok, socket} = :gen_udp.open(port, socket_options)
    {:ok, allocated_port} = :inet.port(socket)
    Logger.info("Ack Receiver - listening on port #{allocated_port}")
    state = %{socket: socket, port: allocated_port}
    {:ok, state}
  end

  def port() do
    GenServer.call(__MODULE__, :get_port)
  end

  def handle_info({:udp, _socket, src_ip, src_port, data}, state) do
    Logger.debug(
      "Ack Receiver - incoming data: #{inspect(data)} from #{inspect(src_ip)}:#{inspect(src_port)}"
    )

    case Jason.decode(data) do
      {:ok,
       %{
         "cmd" => "ack",
         "uuid" => sender_uuid,
         "hostname" => sender_hostname,
         "metadata" => sender_metadata
       }} ->
        Tau5.Discovery.KnownNodes.add_node(
          sender_uuid,
          uuid: sender_uuid,
          hostname: sender_hostname,
          metadata: sender_metadata,
          ip: src_ip
        )

      _ ->
        Logger.error("Ack Receiver failed to decode data: #{inspect(data)}")
    end

    {:noreply, state}
  end

  def handle_call(:get_port, _from, state) do
    {:reply, state.port, state}
  end
end
