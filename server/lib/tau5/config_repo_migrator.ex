defmodule Tau5.ConfigRepoMigrator do
  use GenServer
  require Logger

  def start_link(_) do
    GenServer.start_link(__MODULE__, nil, name: __MODULE__)
  end

  @impl true
  def init(_) do
    # Run migrations synchronously in init
    path = Tau5.Paths.config_repo_migrations_path()

    case Ecto.Migrator.run(Tau5.ConfigRepo, path, :up, all: true) do
      migrations when is_list(migrations) ->
        Logger.info("Ran #{length(migrations)} migrations")

      error ->
        Logger.error("Migration error: #{inspect(error)}")
    end

    {:ok, :migrated}
  end
end
