defmodule Tau5.ConfigRepoMigrator do
  use Task

  def start_link(_) do
    Task.start_link(fn ->
      path = Tau5.Paths.config_repo_migrations_path()

      case Ecto.Migrator.run(Tau5.ConfigRepo, path, :up, all: true) do
        migrations when is_list(migrations) ->
          IO.puts("Ran #{length(migrations)} migrations")

        error ->
          IO.puts("Migration error: #{inspect(error)}")
      end

      # Task exits here naturally
    end)
  end
end
