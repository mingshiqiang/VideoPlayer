@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64

set QT_DIR=D:\DevKits\Qt\6.8.3\msvc2022_64
set PATH=%QT_DIR%\bin;%PATH%

:: 使用独立的 build-debug 目录
cmake -B build-debug -S . -DCMAKE_PREFIX_PATH=%QT_DIR%
cmake --build build-debug --config Debug --parallel

echo Debug Build complete.
pause