defmodule Tau5.ConfigRepo do
  use Ecto.Repo,
    otp_app: :tau5,
    adapter: Ecto.Adapters.SQLite3

  @impl true
  def init(_type, config) do
    {:ok, Keyword.put(config, :database, Tau5.Paths.config_repo_db_path())}
  end
end
