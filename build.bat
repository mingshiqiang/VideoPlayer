@echo off
:: vs2026 build path
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64

set QT_DIR=D:\DevKits\Qt\6.8.3\msvc2022_64
set PATH=%QT_DIR%\bin;%PATH%

cmake -B build -S . -DCMAKE_PREFIX_PATH=%QT_DIR% -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

echo Build complete.
pause