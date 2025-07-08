defmodule Tau5.ConfigRepoMigrator do
  require Logger

  def child_spec(_) do
    %{
      id: __MODULE__,
      start: {__MODULE__, :start_link, []},
      type: :worker,
      restart: :temporary
    }
  end

  def start_link do
    Task.start_link(fn ->
      Logger.info("Running ConfigRepo migrations...")
      path = Tau5.Paths.config_repo_migrations_path()

      case Ecto.Migrator.with_repo(
             Tau5.ConfigRepo,
             &Ecto.Migrator.run(&1, path, :up, all: true)
           ) do
        {:ok, _apps, num} ->
          Logger.info("Ran #{num} migrations")

        {:error, error} ->
          Logger.error("Migration failed: #{inspect(error)}")
      end
    end)
  end
end
