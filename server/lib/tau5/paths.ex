defmodule Tau5.Paths do
  @app_name "Tau5"

  def config_repo_migrations_path do
    path =
      Application.app_dir(:tau5, "priv/config_repo/migrations")
      |> Path.expand()

    IO.puts("Config Repo Migrations Path: #{path}")
    path
  end

  def config_repo_db_path do
    path =
      Path.join([data_dir(), "tau5.db"])
      |> Path.expand()

    IO.puts("Config Repo DB Path: #{path}")
    path
  end

  def data_dir do
    (System.get_env("TAU5_HOME") || platform_data_dir())
    |> Path.expand()
    |> tap(&File.mkdir_p!/1)
  end

  defp platform_data_dir do
    case :os.type() do
      {:win32, _} -> windows_data_dir()
      {:unix, :darwin} -> macos_data_dir()
      {:unix, _} -> linux_data_dir()
      _ -> raise "Unsupported operating system"
    end
  end

  defp windows_data_dir do
    # Use APPDATA for roaming data, LOCALAPPDATA for local-only data
    app_data = System.get_env("APPDATA") || System.get_env("LOCALAPPDATA")

    if app_data do
      Path.join(app_data, @app_name)
    else
      # Fallback to home directory
      Path.join(home_dir(), ".#{String.downcase(@app_name)}")
    end
  end

  defp macos_data_dir do
    Path.join([home_dir(), "Library", "Application Support", @app_name])
  end

  defp linux_data_dir do
    # Follow XDG Base Directory specification
    config_home = System.get_env("XDG_DATA_HOME") || Path.join(home_dir(), ".config")
    Path.join(config_home, String.downcase(@app_name))
  end

  defp home_dir do
    System.user_home!()
  end
end
