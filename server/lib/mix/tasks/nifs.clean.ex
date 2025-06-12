defmodule Mix.Tasks.Nifs.Clean do
  use Mix.Task
  require Logger

  @impl true
  def run(_args) do
    Logger.info("Cleaning NIFs")

    Logger.info("Cleaning SP Link build dir...")
    File.rm_rf!("deps/sp_link/build")
    Logger.info("Cleaning SP MIDI build dir...")
    File.rm_rf!("deps/sp_midi/build")
    Logger.info("Cleaning Tau5 Discovery build dir...")
    File.rm_rf!("deps/tau5_discovery/build")

    File.rm_rf!("priv/nifs")
    File.mkdir_p("priv/nifs")
    Logger.info("NIF cleaning complete")
  end
end
