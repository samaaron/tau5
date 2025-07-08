set WORKING_DIR=%CD%
cd %~dp0

cd ..\..\
cd server
rmdir deps\sp_link/build /s /q
rmdir deps\sp_link\deps\sp_link /s /q
rmdir deps\tau5_discovery\build /s /q
rmdir deps\exqlite\build /s /q
rmdir priv\nifs /s /q
mkdir priv\nifs

rmdir _build /s /q
rmdir deps /s /q
del priv\static\assets\*.* /s /q
for /d %%d in (priv\static\assets\*) do rmdir "%%d" /s /q
cd ..\
rmdir gui\build /s /q
rmdir Release /s /q
rmdir build /s /q

cd %WORKING_DIR%

echo Cleaned up any build directories that were found.
