defmodule Tau5.Discovery do
  use Supervisor
  # The default multicast address is the same as the one used by Ableton's Link protocol
  # The default port is one higher than that used by Link.
  @default_multicast_addr {224, 76, 78, 75}
  @default_discovery_port 20809
  @default_metadata %{}

  def start_link(args) do
    Supervisor.start_link(__MODULE__, args, name: __MODULE__)
  end

  @impl true
  def init(args) when is_map(args) do
    info =
      args
      |> Map.put_new(:metadata, @default_metadata)
      |> Map.put_new(:multicast_addr, @default_multicast_addr)
      |> Map.put_new(:discovery_port, @default_discovery_port)
      |> Map.put_new(:hostname, gethostname())
      |> Map.put_new(:uuid, UUID.uuid4())

    children = [
      Tau5.Discovery.KnownNodes,
      Tau5.Discovery.ReceiverSupervisor,
      Tau5.Discovery.BroadcastSupervisor,
      {Tau5.Discovery.NetworkInterfaceWatcher, info}
    ]

    Supervisor.init(children, strategy: :rest_for_one)
  end

  def nodes do
    Tau5.Discovery.KnownNodes.nodes()
  end

  defp gethostname do
    case :inet.gethostname() do
      {:ok, host} -> to_string(host)
      {:error, _reason} -> "unknown-host"
    end
  end
end
