defmodule Tau5.Settings.AppSetting do
  use Ecto.Schema
  import Ecto.Changeset

  @primary_key {:key, :string, autogenerate: false}
  schema "app_settings" do
    field(:value, :string)
    timestamps()
  end

  def changeset(setting, attrs) do
    setting
    |> cast(attrs, [:key, :value])
    |> validate_required([:key, :value])
    |> validate_length(:key, max: 64)
    |> unique_constraint(:key)

    # No put_change/2 for updated_at — Ecto fills it automatically.
  end
end
