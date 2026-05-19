@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%
set DEPS_DIR=%SCRIPT_DIR%\deps
set IMGUI_DIR=%DEPS_DIR%\imgui
set GLFW_DIR=%DEPS_DIR%\glfw-mingw
set BIN_DIR=%SCRIPT_DIR%\bin\windows
set EXE=tomewell.exe

set CXX=g++
set CC=gcc
set BASE_FLAGS=-O2 -I"%IMGUI_DIR%" -I"%IMGUI_DIR%/backends" -I"%GLFW_DIR%/include" -I"%DEPS_DIR%" -I"%DEPS_DIR%/tinyfiledialogs"
set CXXFLAGS=-std=c++11 %BASE_FLAGS%
set CFLAGS=%BASE_FLAGS%
set CXXFLAGS_STATIC=-static-libstdc++ -static-libgcc
set LIBS=-L"%GLFW_DIR%/lib-mingw-w64" -lglfw3 -lgdi32 -lopengl32 -limm32 -lole32 -lcomdlg32

echo ^=^=^> Checking for MinGW-w64...
where %CXX% >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo %CXX% not found. Install MinGW-w64 from https://www.mingw-w64.org/
    echo or via MSYS2: pacman -S mingw-w64-x86_64-toolchain
    pause
    exit /b 1
)
echo   Found %CXX%

echo ^=^=^> Checking for GLFW (MinGW)...
if not exist "%GLFW_DIR%\lib-mingw-w64\libglfw3.a" (
    echo   Downloading pre-built GLFW for MinGW-w64...
    if not exist "%DEPS_DIR%" mkdir "%DEPS_DIR%"
    set GLFW_URL=https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip
    set TMPZIP=%TEMP%\glfw-mingw.zip
    curl.exe -fsSL "!GLFW_URL!" -o "!TMPZIP!" || (
        powershell -Command "Invoke-WebRequest -Uri '!GLFW_URL!' -OutFile '!TMPZIP!'"
    )
    powershell -Command "Expand-Archive -Path '!TMPZIP!' -DestinationPath '%DEPS_DIR%' -Force"
    if exist "%DEPS_DIR%\glfw-3.4.bin.WIN64" (
        xcopy /E /I /Y "%DEPS_DIR%\glfw-3.4.bin.WIN64\*" "%GLFW_DIR%\" >nul
        rmdir /S /Q "%DEPS_DIR%\glfw-3.4.bin.WIN64"
    )
    del "!TMPZIP!" 2>nul
    echo   GLFW for MinGW downloaded.
) else (
    echo   GLFW found at %GLFW_DIR%
)

echo ^=^=^> Compiling...
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
set FULL_EXE=%BIN_DIR%\%EXE%

set OBJS=

echo   %CC% -c deps/tinyfiledialogs/tinyfiledialogs.c
%CC% %CFLAGS% -c "%DEPS_DIR%/tinyfiledialogs/tinyfiledialogs.c" -o tinyfiledialogs.o
set OBJS=%OBJS% tinyfiledialogs.o

for %%F in (imgui.cpp imgui_demo.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp) do (
    echo   %CXX% -c deps/imgui/%%F
    %CXX% %CXXFLAGS% %CXXFLAGS_STATIC% -c "%IMGUI_DIR%/%%F" -o %%~nF.o
    set OBJS=!OBJS! %%~nF.o
)

for %%F in (imgui_impl_glfw.cpp imgui_impl_opengl3.cpp) do (
    echo   %CXX% -c backends/%%F
    %CXX% %CXXFLAGS% %CXXFLAGS_STATIC% -c "%IMGUI_DIR%/backends/%%F" -o %%~nF.o
    set OBJS=!OBJS! %%~nF.o
)

echo   %CXX% -c main.cpp
%CXX% %CXXFLAGS% %CXXFLAGS_STATIC% -c "%SCRIPT_DIR%/main.cpp" -o main.o
set OBJS=%OBJS% main.o

echo ^=^=^> Linking...
%CXX% %CXXFLAGS_STATIC% -o "%FULL_EXE%" %OBJS% %LIBS%

echo ^=^=^> Cleaning up .o files...
del /Q *.o 2>nul

echo ^=^=^> Copying translations...
if exist "%BIN_DIR%\translations" rmdir /S /Q "%BIN_DIR%\translations"
xcopy /E /I /Y "%SCRIPT_DIR%\translations\done\*" "%BIN_DIR%\translations\" >nul

echo ^=^=^> Copying help file...
copy /Y "%SCRIPT_DIR%\tomewell_help.html" "%BIN_DIR%\" >nul

echo ^=^=^> Done: %FULL_EXE%
echo     (copy the whole bin\windows\ folder to another machine to run)

endlocal
