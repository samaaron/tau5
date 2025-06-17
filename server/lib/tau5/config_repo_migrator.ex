defmodule Tau5.ConfigRepoMigrator do
  use Task, restart: :transient

  def start_link(_arg) do
    Task.start_link(fn ->
      path = Tau5.Paths.config_repo_migrations_path()

      Ecto.Migrator.with_repo(
        Tau5.ConfigRepo,
        &Ecto.Migrator.run(&1, path, :up, all: true)
      )
    end)
  end
end
