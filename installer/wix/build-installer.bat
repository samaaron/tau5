set WORKING_DIR=%CD%
cd %~dp0

REM uses wix v3
heat dir ..\..\Release -gg -g1 -sreg -sfrag -dr INSTALLFOLDER -cg Tau5AppComponent -out ReleaseFiles.wxs
candle tau5.wxs ReleaseFiles.wxs  -ext WixUtilExtension -arch x64
light tau5.wixobj ReleaseFiles.wixobj -o Tau5Installer.msi -b ..\..\Release -ext WixUIExtension -ext WixUtilExtension




cd %WORKING_DIR%
