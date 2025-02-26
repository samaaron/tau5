defmodule Mix.Tasks.SpNifs.Clean do
  use Mix.Task
  require Logger

  @impl true
  def run(_args) do
    Logger.info("Cleaning SP Nifs")

    Logger.info("Cleaning SP Link build dir...")
    File.rm_rf!("deps/sp_link/build")
    Logger.info("Cleaning SP MIDI build dir...")
    File.rm_rf!("deps/sp_midi/build")

    File.rm_rf!("priv/sp_nifs")
    File.mkdir_p("priv/sp_nifs")
    File.touch!("priv/sp_nifs/.gitkeep")
    Logger.info("Cleaning SP Nifs complete")
  end
end
