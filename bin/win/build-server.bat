set WORKING_DIR=%CD%

cd %~dp0\..\..\server


SET MIX_ENV=prod

cmd /c mix local.hex --force
cmd /c mix local.rebar --force
cmd /c mix setup
cmd /c mix assets.setup
cmd /c mix assets.deploy
cmd /c mix release --overwrite --no-deps-check

cd %WORKING_DIR%