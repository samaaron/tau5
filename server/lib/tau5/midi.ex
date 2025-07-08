defmodule Tau5.MIDI do
  use GenServer
  alias Phoenix.PubSub
  require Logger

  def start_link(args) do
    GenServer.start_link(__MODULE__, args, name: __MODULE__)
  end

  @impl true
  def init(_args) do
    Logger.info("Starting MIDI")

    if Application.get_env(:tau5, :midi_enabled, true) do
      Logger.info("SP MIDI NIF loaded: #{inspect(:sp_midi.is_nif_loaded())} ")
      :sp_midi.midi_init()
      :sp_midi.have_my_pid()
    else
      Logger.info("MIDI NIF disabled")
    end

    {:ok, []}
  end

  @impl true
  def handle_info({:midi_in, port_name, binary}, state) do
    Logger.info("MIDId input received from #{port_name}: #{inspect(binary)}")
    PubSub.broadcast(Tau5.PubSub, "midi-input", :midi_input)
    {:noreply, state}
  end
end
