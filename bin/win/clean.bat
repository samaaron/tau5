set WORKING_DIR=%CD%
cd %~dp0

cd ..\..\
cd server
cmd /c mix clean --deps
rmdir _build /s /q
rmdir deps /s /q
del priv\static\assets\*.* /s /q
for /d %%d in (priv\static\assets\*) do rmdir "%%d" /s /q
cd ..\
rmdir app\build /s /q
rmdir Release /s /q
rmdir build /s /q


cd %WORKING_DIR%
