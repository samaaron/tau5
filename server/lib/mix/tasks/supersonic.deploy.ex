defmodule Mix.Tasks.Supersonic.Deploy do
  @moduledoc """
  Copies SuperSonic runtime assets from assets/vendor to priv/static.

  This task copies WASM modules and worker files that SuperSonic loads at runtime.
  Samples and synthdefs remain in priv/static as they are Tau5-owned assets.

  ## Usage

      mix supersonic.deploy

  ## Development Workflow

  For active SuperSonic development, symlink the entire supersonic repo:

      cd server/assets/vendor
      rm -rf supersonic
      ln -sf ../../../supersonic supersonic

  This gives you the full repo structure with dist/ folder.
  Then run this task to copy the built files to priv/static.
  """

  use Mix.Task

  @shortdoc "Deploys SuperSonic runtime assets to priv/static"

  @impl Mix.Task
  def run(_args) do
    source_wasm = Path.expand("assets/vendor/supersonic/dist/wasm")
    source_workers = Path.expand("assets/vendor/supersonic/dist/workers")

    dest_wasm = Path.expand("priv/static/supersonic/dist/wasm")
    dest_workers = Path.expand("priv/static/supersonic/dist/workers")

    Mix.shell().info("Deploying SuperSonic runtime assets...")

    # Create destination directories
    File.mkdir_p!(dest_wasm)
    File.mkdir_p!(dest_workers)

    # Copy WASM files
    if File.exists?(source_wasm) do
      copy_directory(source_wasm, dest_wasm)
      Mix.shell().info("✓ Copied WASM files")
    else
      Mix.shell().error("✗ Source WASM directory not found: #{source_wasm}")
    end

    # Copy worker files
    if File.exists?(source_workers) do
      copy_directory(source_workers, dest_workers)
      Mix.shell().info("✓ Copied worker files")
    else
      Mix.shell().error("✗ Source workers directory not found: #{source_workers}")
    end

    Mix.shell().info("SuperSonic assets deployed successfully!")
  end

  defp copy_directory(source, dest) do
    source
    |> File.ls!()
    |> Enum.each(fn file ->
      source_path = Path.join(source, file)
      dest_path = Path.join(dest, file)

      cond do
        File.dir?(source_path) ->
          File.mkdir_p!(dest_path)
          copy_directory(source_path, dest_path)

        # Skip copying symlinks themselves, copy their targets
        File.lstat!(source_path).type == :symlink ->
          real_path = File.read_link!(source_path)
          # If symlink is relative, make it absolute from source dir
          real_path =
            if Path.type(real_path) == :relative do
              Path.expand(real_path, Path.dirname(source_path))
            else
              real_path
            end

          if File.exists?(real_path) do
            File.cp!(real_path, dest_path)
          end

        true ->
          File.cp!(source_path, dest_path)
      end
    end)
  end
end
