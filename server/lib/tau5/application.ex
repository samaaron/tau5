defmodule Tau5.Application do
  use Application
  require Logger

  @impl true
  def start(_type, _args) do
    # Start ConfigRepo first
    {:ok, _} =
      Supervisor.start_link([Tau5.ConfigRepo],
        strategy: :one_for_one,
        name: Tau5.ConfigRepoSupervisor
      )

    # Run migrations synchronously
    run_migrations!()

    # Now start everything else
    http_port = Application.get_env(:tau5, Tau5Web.Endpoint)[:http][:port]

    children = [
      # ConfigRepoMigrator removed from here!
      Tau5Web.Telemetry,
      {Phoenix.PubSub, name: Tau5.PubSub},
      {Finch, name: Tau5.Finch},
      Tau5Web.Endpoint,
      Tau5.Link,
      Tau5.MIDI,
      {Tau5.Discovery, %{http_port: http_port}}
    ]

    opts = [strategy: :one_for_one, name: Tau5.Supervisor]
    Supervisor.start_link(children, opts)
  end

  defp run_migrations! do
    path = Tau5.Paths.config_repo_migrations_path()

    case Ecto.Migrator.with_repo(
           Tau5.ConfigRepo,
           &Ecto.Migrator.run(&1, path, :up, all: true)
         ) do
      {:ok, _apps, num} ->
        Logger.info("Ran #{num} ConfigRepo migrations")

      {:error, error} ->
        raise "Failed to run migrations: #{inspect(error)}"
    end
  end

  @impl true
  def config_change(changed, _new, removed) do
    Tau5Web.Endpoint.config_change(changed, removed)
    :ok
  end
end
