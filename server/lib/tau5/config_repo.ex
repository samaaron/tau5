defmodule Tau5.ConfigRepo do
  use Ecto.Repo,
    otp_app: :tau5,
    adapter: Ecto.Adapters.SQLite3

  @impl true
  def init(_type, config) do
    # Use configured database if provided (like :memory: in tests)
    # Otherwise use the file path
    if config[:database] do
      {:ok, config}
    else
      {:ok, Keyword.put(config, :database, Tau5.Paths.config_repo_db_path())}
    end
  end
end
