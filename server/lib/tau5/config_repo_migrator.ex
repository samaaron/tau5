defmodule Tau5.ConfigRepoMigrator do
  def child_spec(_) do
    %{
      id: __MODULE__,
      start: {__MODULE__, :run, []},
      type: :worker,
      restart: :transient
    }
  end

  # called by the supervisor
  def run do
    path = Tau5.Paths.config_repo_migrations_path()

    Ecto.Migrator.with_repo(
      Tau5.ConfigRepo,
      &Ecto.Migrator.run(&1, path, :up, all: true)
    )

    :ignore
  end
end
