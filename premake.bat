@echo off
"bin/windows/premake5.exe" gmake
"bin/windows/premake5.exe" --cc=clang gmake
"bin/windows/premake5.exe" vs2017
pause
