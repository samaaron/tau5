defmodule Mix.Tasks.SpNifs.Compile do
  use Mix.Task
  require Logger

  @impl true
  def run(_args) do
    IO.puts("Compiling SP Link")

    case :os.type() do
      {:unix, :darwin} -> compile(:macos, arch())
      {:unix, :linux} -> compile(:linux, arch())
      {:unix, :freebsd} -> compile(:linux, arch())
      {:unix, :openbsd} -> compile(:linux, arch())
      {:win32, :nt} -> compile(:win, arch())
      unknown_os -> {:unsupported_system, unknown_os}
    end
  end

  def compile(platform, arch) do
    compile_spmidi(platform, arch)
    compile_splink(platform, arch)
  end

  defp compile_win_x64(proj) do
    Logger.info("Compiling #{proj} for Windows x64")
    File.mkdir_p("deps/#{proj}/build")

    File.cd!("deps/#{proj}/build", fn ->
      {cmake_output, cmake_status} =
        System.cmd("cmake", [
          "-G",
          "Visual Studio 17 2022",
          "-A",
          "x64",
          "-DCMAKE_INSTALL_PREFIX=../../../priv/nif",
          "-DCMAKE_BUILD_TYPE=Release",
          ".."
        ])

      Logger.debug("CMake output:\n#{cmake_output}")
      if cmake_status != 0, do: raise("CMake failed with status #{cmake_status}")

      {cmake_output, cmake_status} =
        System.cmd("cmake", ["--build", ".", "--config", "Release"])

      Logger.debug("CMake output:\n#{cmake_output}")
      if cmake_status != 0, do: raise("CMake failed with status #{cmake_status}")

      {cmake_output, cmake_status} =
        System.cmd("cmake", ["--install", "."])

      Logger.debug("CMake output:\n#{cmake_output}")
      if cmake_status != 0, do: raise("CMake failed with status #{cmake_status}")

      Logger.info("Building #{proj} for Win x64 complete")
    end)
  end

  defp compile_mac_arm64(proj) do
    Logger.info("Compiling #{proj} for macOS arm64")
    File.mkdir_p("deps/#{proj}/build")

    File.cd!("deps/#{proj}/build", fn ->
      {cmake_output, cmake_status} =
        System.cmd("cmake", [
          "-G",
          "Unix Makefiles",
          "-DCMAKE_OSC_ARCHITECTURES=ARM64",
          "-DCMAKE_INSTALL_PREFIX=../../../priv/nif",
          "-DCMAKE_OSX_DEPLOYMENT_TARGET=12",
          "-DCMAKE_BUILD_TYPE=Release",
          ".."
        ])

      Logger.debug("CMake output:\n#{cmake_output}")
      if cmake_status != 0, do: raise("CMake failed with status #{cmake_status}")

      {cmake_output, cmake_status} =
        System.cmd("cmake", ["--build", ".", "--config", "Release"])

      Logger.debug("CMake output:\n#{cmake_output}")
      if cmake_status != 0, do: raise("CMake failed with status #{cmake_status}")

      {cmake_output, cmake_status} =
        System.cmd("cmake", ["--install", "."])

      Logger.debug("CMake output:\n#{cmake_output}")
      if cmake_status != 0, do: raise("CMake failed with status #{cmake_status}")

      Logger.info("Building #{proj} for macOS arm64 complete")
    end)
  end

  defp compile_spmidi(:win, :x64) do
    compile_win_x64("sp_midi")
  end

  defp compile_spmidi(:macos, :arm64) do
    compile_mac_arm64("sp_midi")
  end

  defp compile_spmidi(os, arch) do
    Logger.info("Uknown OS or architecture to compile spmidi for: #{inspect([os, arch])}")
  end

  defp compile_splink(:win, :x64) do
    compile_win_x64("sp_link")
  end

  defp compile_splink(:macos, :arm64) do
    compile_mac_arm64("sp_link")
  end

  defp compile_splink(os, arch) do
    Logger.info("Uknown OS or architecture to compile splink for: #{inspect([os, arch])}")
  end

  defp arch do
    arch_desc = to_string(:erlang.system_info(:system_architecture))

    case arch_desc do
      "aarch64" <> _ -> :arm64
      _ -> :x64
    end
  end
end
