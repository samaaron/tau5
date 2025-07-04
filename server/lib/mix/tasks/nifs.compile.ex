defmodule Mix.Tasks.Nifs.Compile do
  use Mix.Task
  require Logger

  @impl true
  def run(_args) do
    IO.puts("Compiling NIFs for sp_midi, sp_link, and tau5_discovery...")

    IO.puts("Now compiling NIFs...")

    case :os.type() do
      {:unix, :darwin} -> compile(:macos, arch())
      {:unix, :linux} -> compile(:linux, arch())
      {:unix, :freebsd} -> compile(:linux, arch())
      {:unix, :openbsd} -> compile(:linux, arch())
      {:win32, :nt} -> compile(:win, arch())
      unknown_os -> {:unsupported_system, unknown_os}
    end
  end

  def patch_exqlite do
    exqlite_path = "deps/exqlite"
    File.cp!("etc/exqlite_CMakeLists.txt", "#{exqlite_path}/CMakeLists.txt")
    File.mkdir_p("#{exqlite_path}/c_include")
    File.cp_r!("etc/erlang_headers", "#{exqlite_path}/c_include/erlang_headers")
  end

  def compile(platform, arch) do
    compile_spmidi(platform, arch)
    compile_splink(platform, arch)
    compile_tau5_discovery(platform, arch)

    ## exqlite typically ships with precompiled binaries
    ## If we want to manually compile it, we need to patch it to support cmake first
    ## IO.puts("First, patching exqlite for cmake support...")
    ## patch_exqlite()
    ## compile_exqlite(platform, arch)
  end

  defp cmake_build_and_install(
         build_dir: build_dir,
         install_dir: install_dir,
         cmake_args: cmake_args
       ) do
    File.mkdir_p(build_dir)

    cmake_args =
      [
        "-DCMAKE_INSTALL_PREFIX=#{install_dir}",
        "-DCMAKE_BUILD_TYPE=Release",
        ".."
      ] ++ cmake_args

    File.cd!(build_dir, fn ->
      {cmake_output, cmake_status} =
        System.cmd("cmake", cmake_args)

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

      Logger.info("Building #{build_dir} complete")
    end)
  end

  defp compile_lin_arm64(proj) do
    Logger.info("Compiling #{proj} for Linux arm64")
    proj_base_dir = "deps/#{proj}"
    proj_build_dir = "#{proj_base_dir}/build"
    File.mkdir_p(proj_build_dir)

    cmake_build_and_install(
      build_dir: proj_build_dir,
      install_dir: "../../../priv/nifs",
      cmake_args: [
        "-G",
        "Unix Makefiles",
        "-DCMAKE_OSC_ARCHITECTURES=ARM64"
      ]
    )
  end

  defp compile_lin_x64(proj) do
    Logger.info("Compiling #{proj} for Linux x64")
    proj_base_dir = "deps/#{proj}"
    proj_build_dir = "#{proj_base_dir}/build"
    File.mkdir_p(proj_build_dir)

    cmake_build_and_install(
      build_dir: proj_build_dir,
      install_dir: "../../../priv/nifs",
      cmake_args: [
        "-G",
        "Unix Makefiles",
        "-DCMAKE_OSC_ARCHITECTURES=x86_64"
      ]
    )
  end

  defp compile_win_arm64(proj) do
    Logger.info("Compiling #{proj} for Windows arm64")
    proj_base_dir = "deps/#{proj}"
    proj_build_dir = "#{proj_base_dir}/build"
    File.mkdir_p(proj_build_dir)

    cmake_build_and_install(
      build_dir: proj_build_dir,
      install_dir: "../../../priv/nifs",
      cmake_args: [
        "-G",
        "Visual Studio 17 2022",
        "-A",
        "ARM64"
      ]
    )

    Logger.info("Building #{proj} for Win arm64 complete")
  end

  defp compile_win_x64(proj) do
    Logger.info("Compiling #{proj} for Windows x64")
    proj_base_dir = "deps/#{proj}"
    proj_build_dir = "#{proj_base_dir}/build"
    File.mkdir_p(proj_build_dir)

    cmake_build_and_install(
      build_dir: proj_build_dir,
      install_dir: "../../../priv/nifs",
      cmake_args: [
        "-G",
        "Visual Studio 17 2022",
        "-A",
        "x64"
      ]
    )

    Logger.info("Building #{proj} for Win x64 complete")
  end

  defp compile_mac_arm64(proj) do
    Logger.info("Compiling #{proj} for macOS arm64")
    proj_base_dir = "deps/#{proj}"
    proj_build_dir = "#{proj_base_dir}/build"
    File.mkdir_p(proj_build_dir)

    cmake_build_and_install(
      build_dir: proj_build_dir,
      install_dir: "../../../priv/nifs",
      cmake_args: [
        "-G",
        "Unix Makefiles",
        "-DCMAKE_OSC_ARCHITECTURES=ARM64",
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=12"
      ]
    )

    Logger.info("Building #{proj} for macOS arm64 complete")
  end

  defp compile_spmidi(:linux, :x64) do
    compile_lin_x64("sp_midi")
  end

  defp compile_spmidi(:linux, :arm64) do
    compile_lin_arm64("sp_midi")
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

  defp compile_splink(:linux, :x64) do
    compile_lin_x64("sp_link")
  end

  defp compile_splink(:linux, :arm64) do
    compile_lin_arm64("sp_link")
  end

  defp compile_splink(:win, :arm64) do
    compile_win_arm64("sp_link")
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

  defp compile_tau5_discovery(:linux, :x64) do
    compile_lin_x64("tau5_discovery")
  end

  defp compile_tau5_discovery(:linux, :arm64) do
    compile_lin_arm64("tau5_discovery")
  end

  defp compile_tau5_discovery(:win, :arm64) do
    compile_win_arm64("tau5_discovery")
  end

  defp compile_tau5_discovery(:win, :x64) do
    compile_win_x64("tau5_discovery")
  end

  defp compile_tau5_discovery(:macos, :arm64) do
    compile_mac_arm64("tau5_discovery")
  end

  defp compile_tau5_discovery(os, arch) do
    Logger.info("Uknown OS or architecture to compile tau5_discovery for: #{inspect([os, arch])}")
  end

  defp compile_exqlite(:linux, :x64) do
    compile_lin_x64("exqlite")
  end

  defp compile_exqlite(:linux, :arm64) do
    compile_lin_arm64("exqlite")
  end

  defp compile_exqlite(:win, :arm64) do
    compile_win_arm64("exqlite")
  end

  defp compile_exqlite(:win, :x64) do
    compile_win_x64("exqlite")
  end

  defp compile_exqlite(:macos, :arm64) do
    compile_mac_arm64("exqlite")
  end

  defp compile_exqlite(os, arch) do
    Logger.info("Unknown OS or architecture to compile exqlite for: #{inspect([os, arch])}")
  end

  defp arch do
    arch_desc = to_string(:erlang.system_info(:system_architecture))

    case arch_desc do
      "aarch64" <> _ -> :arm64
      _ -> :x64
    end
  end
end
