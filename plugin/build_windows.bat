@echo off
rem =========================================================================
rem  Builds Obsidian Suite (VST3 + standalone) on your own Windows PC.
rem  Needs: Visual Studio 2022 (with "Desktop development with C++"), CMake,
rem  Git. Run from this folder:   build_windows.bat
rem  Result: build\ObsidianSuite_artefacts\Release\VST3 and \Standalone
rem =========================================================================
setlocal
cd /d "%~dp0"

rem WebView2 SDK (the UI runs in Edge WebView2). Downloaded once into .\build\nuget
if not exist build\nuget\Microsoft.Web.WebView2* (
  echo Downloading the WebView2 SDK...
  if not exist build mkdir build
  powershell -NoProfile -Command "Invoke-WebRequest https://dist.nuget.org/win-x86-commandline/latest/nuget.exe -OutFile build\nuget.exe" || goto :fail
  build\nuget.exe install Microsoft.Web.WebView2 -Version 1.0.1901.177 -OutputDirectory build\nuget -NonInteractive || goto :fail
)

cmake -B build -A x64 -DJUCE_WEBVIEW2_PACKAGE_LOCATION="%cd%\build\nuget" || goto :fail
cmake --build build --config Release --parallel --target ObsidianSuite_VST3 ObsidianSuite_Standalone || goto :fail

echo.
echo Running the self-test...
start /wait "" "build\ObsidianSuite_artefacts\Release\Standalone\Obsidian Suite.exe" --selftest build\selftest.txt
type build\selftest.txt
echo.
echo Done. Copy "build\ObsidianSuite_artefacts\Release\VST3\Obsidian Suite.vst3"
echo to "C:\Program Files\Common Files\VST3" (or use the installer from GitHub).
exit /b 0

:fail
echo.
echo Build failed. See the messages above.
exit /b 1
