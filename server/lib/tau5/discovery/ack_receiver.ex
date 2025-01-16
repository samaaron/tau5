defmodule Tau5.Discovery.AckReceiver do
  use GenServer, restart: :transient
  require Logger

  def start_link(info) do
    GenServer.start_link(__MODULE__, info)
  end

  @impl true
  def init({interface, token}) do
    socket_options = [
      :binary,
      {:active, true}
    ]

    {:ok, socket} = :gen_udp.open(0, socket_options)
    {:ok, allocated_port} = :inet.port(socket)

    Logger.info(
      "Discovery receiver started for interface #{inspect(interface)} - listening on port #{inspect(allocated_port)}"
    )

    state = %{socket: socket, port: allocated_port, token: token, interface: interface}
    {:ok, state}
  end

  def port(pid) do
    GenServer.call(pid, :get_port)
  end

  def interface(pid) do
    GenServer.call(pid, :get_interface)
  end

  @impl true
  def handle_call(:get_interface, _from, state) do
    {:reply, state.interface, state}
  end

  @impl true
  def handle_call(:get_port, _from, state) do
    {:reply, state.port, state}
  end

  @impl true
  def handle_info({:udp, _socket, src_ip, src_port, data}, %{token: token} = state) do
    Logger.debug(
      "Ack Receiver - incoming data: #{inspect(data)} from #{inspect(src_ip)}:#{inspect(src_port)}"
    )

    case Jason.decode(data) do
      {:ok,
       %{
         "cmd" => "ack",
         "uuid" => sender_uuid,
         "hostname" => sender_hostname,
         "metadata" => sender_metadata,
         "token" => ^token
       }} ->
        Tau5.Discovery.KnownNodes.add_node(
          sender_uuid,
          state.interface,
          hostname: sender_hostname,
          metadata: sender_metadata,
          ip: src_ip
        )

      _ ->
        Logger.error("Acker Receiver failed to decode data: #{inspect(data)}")
    end

    {:noreply, state}
  end
end
