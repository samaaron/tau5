defmodule Tau5.Discovery do
  use Supervisor

  def start_link(args) do
    Supervisor.start_link(__MODULE__, args, name: __MODULE__)
  end

  @impl true
  def init(args) do
    children = [
      Tau5.Discovery.KnownNodes,
      Tau5.Discovery.Receiver,
      Tau5.Discovery.BroadcastSupervisor,
      {Tau5.Discovery.NetworkInterfaceWatcher, args}
    ]

    Supervisor.init(children, strategy: :rest_for_one)
  end

  def nodes do
    Tau5.Discovery.KnownNodes.nodes()
  end
end
