defmodule Tau5.Settings do
  alias Tau5.{ConfigRepo, Settings.AppSetting}

  def get(key, default \\ nil) do
    case ConfigRepo.get(AppSetting, key) do
      %AppSetting{value: value} -> value
      nil -> default
    end
  end

  def put(key, value) when is_binary(key) and is_binary(value) do
    %AppSetting{}
    |> AppSetting.changeset(%{key: key, value: value})
    |> ConfigRepo.insert(
      on_conflict: {:replace, [:value, :updated_at]},
      conflict_target: :key,
      # get fresh timestamps back
      returning: true
    )
  end

  def get_or_put(key, default) when is_binary(key) and is_binary(default) do
    case ConfigRepo.insert(
           %AppSetting{key: key, value: default},
           on_conflict: :nothing,
           conflict_target: :key,
           returning: [:value]
         ) do
      # We inserted; RETURNING gave us the value
      {:ok, %AppSetting{value: value}} when not is_nil(value) ->
        value

      # The row already existed; SQLite returned nothing
      {:ok, _} ->
        ConfigRepo.get!(AppSetting, key).value

      # Other problems (invalid changeset, FK breakage…)
      {:error, changeset} ->
        raise "Unable to store default for #{inspect(key)}: #{inspect(changeset)}"
    end
  end

  def all, do: ConfigRepo.all(AppSetting)
end
