defmodule Tau5.ConfigRepo.Migrations.Init do
  use Ecto.Migration

  def change do
    create table(:app_settings, primary_key: false) do
      add :key,   :text, primary_key: true
      add :value, :text, null: false
      timestamps(updated_at: :updated_at)  # optional audit trail
    end

    create unique_index(:app_settings, [:key])

  end
end
