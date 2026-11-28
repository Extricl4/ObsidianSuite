@echo off
rem ORPHEUS: builds the VST3 and the app with the newest Visual Studio (2022, "Desktop development with C++").
rem The WebView2 SDK is fetched with nuget (https://www.nuget.org/downloads: put nuget.exe next to this file or on PATH).
cd /d "%~dp0"
if not exist build\webview2 nuget install Microsoft.Web.WebView2 -Version 1.0.1901.177 -OutputDirectory build\webview2 -NonInteractive
cmake -B build -A x64 -DJUCE_WEBVIEW2_PACKAGE_LOCATION="%~dp0build\webview2" || goto :err
cmake --build build --config Release --parallel --target Orpheus_VST3 Orpheus_Standalone OrpheusTest || goto :err
build\OrpheusTest_artefacts\Release\OrpheusTest.exe build\test-out Tests\presets.json || goto :err
echo.
echo Done:
echo   VST3: build\Orpheus_artefacts\Release\VST3\ORPHEUS.vst3   (copy to C:\Program Files\Common Files\VST3)
echo   App:  build\Orpheus_artefacts\Release\Standalone\ORPHEUS.exe
exit /b 0
:err
echo Build failed.
exit /b 1
